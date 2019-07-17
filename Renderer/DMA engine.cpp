#include "stdafx.h"
#include "DMA engine.h"
#include "texture.hh"
#include "event handle.h"
#include "align.h"

using namespace std;
using namespace Renderer;
using namespace DMA::Impl;
using DMA::WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
extern ComPtr<ID3D12CommandQueue> gfxQueue, dmaQueue;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

static constexpr auto defaultChunkSize = /*128ul*/32ul * 1024ul * 1024ul;
static constexpr auto batchLenLimit = /*1024u*/256u;

static UINT64 lastBatchID;
static UINT64 curBatchSuballocOffset;
static UINT curBatchLen;

static Impl::EventHandle fenceEvent;
static recursive_mutex mtx;

struct Batch
{
	ComPtr<ID3D12Resource> chunk;
	unsigned long chunkVersion;
	vector<ComPtr<ID3D12Resource>> outstandingRefs;
};
static Batch uploadQueue[size(cmdBuffers)];

static inline unsigned LastRingIdx() noexcept
{
	return lastBatchID - 1/*batchID starts with 1*/ & 1u;
}

static inline unsigned CurRingIdx() noexcept
{
	return lastBatchID/*+1[curBatchID] -1[to ring idx (batchID starts with 1)]*/ & 1u;
}

static void WaitForGPU(UINT64 batchID)
{
	if (fence->GetCompletedValue() < batchID)
	{
		CheckHR(fence->SetEventOnCompletion(batchID, fenceEvent));
		if (WaitForSingleObject(fenceEvent, INFINITE) == WAIT_FAILED)
			throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
	}
}

template<bool enableSync/*for new batch*/>
static inline const auto &GetCmdList()
{
	const auto &curCmdBuff = cmdBuffers[CurRingIdx()];
	if constexpr (enableSync)
	{
		if (!curBatchLen && lastBatchID >= size(cmdBuffers))	// started new batch but not initial (cmd list inited during creation)
		{
			WaitForGPU(lastBatchID + 1/*=curButchID*/ - size(cmdBuffers));
			CheckHR(curCmdBuff.allocator->Reset());
			CheckHR(curCmdBuff.list->Reset(curCmdBuff.allocator.Get(), NULL));
		}
	}
	else
		assert(curBatchLen);
	return curCmdBuff.list;
}

static void CleanupFinishedUploads()
{
	// for initial run (lastBatchID == 0) unsigned comparison '0 >= 0 + 1 - size' evaluates to false which is acceptable, nothing to clear yet
	if (const auto finishedBatchID = fence->GetCompletedValue(); finishedBatchID >= lastBatchID + 1/*=curButchID*/ - size(cmdBuffers))
	{
		// if new batch started CurRingIdx() wraps around ringbuffer size
		if (!curBatchLen)
			uploadQueue[CurRingIdx()].outstandingRefs.clear();

		// cleanup ringbuffer for batches until current, hardcoded for buffer size = 2 (i.e. clear 1 entry)
		if (finishedBatchID >= lastBatchID)
			uploadQueue[LastRingIdx()].outstandingRefs.clear();
	}
}

static void FinishCurBatch()
{
	assert(curBatchSuballocOffset && curBatchLen);
	
	// keep chunk ref
	auto &curBatch = uploadQueue[CurRingIdx()];
	curBatch.outstandingRefs.push_back(curBatch.chunk);
	
	const auto &cmdList = GetCmdList<false>();
	CheckHR(cmdList->Close());
	dmaQueue->ExecuteCommandLists(1, CommandListCast(cmdList.GetAddressOf()));
	CheckHR(dmaQueue->Signal(fence.Get(), ++lastBatchID));
	/*
	could just wait for previous batch completion here and reset cmd alloc & list
	instead use more complicated scheme 'GetCmdList' to defer waiting as much as possible
		it allows for more job to be done before waiting and reduces chances waiting to be required ever
	*/
	curBatchSuballocOffset = curBatchLen = 0;
}

static void FlushPendingUploads(bool cleanup)
{
	/*
		not actually needed ?
		no pending uploads after waiting for them (thus no asyncs to sync with) if called from 'FinishLoads()'
		keep it for robustness / possible future arch changes
	*/
	lock_guard lck(mtx);

	if (curBatchLen)
		FinishCurBatch();

	// release DMA buffers, no uploads expected in near future
	if (cleanup)
		for (auto &batch : uploadQueue)
			batch.chunk.Reset();
}

extern void __cdecl FinishLoads()
{
	Texture::WaitForPendingLoads();
	FlushPendingUploads(true);
}

// wait for completion and free upload resources, implies 'FinishLoads()' called before
extern void __cdecl ForceLoadsCompletion()
{
	lock_guard lck(mtx);
	assert(!curBatchLen);
	assert(Texture::PendingLoadsCompleted());
	WaitForGPU(lastBatchID);
	CleanupFinishedUploads();
}

decltype(cmdBuffers) DMA::Impl::CreateCmdBuffers()
{
	decltype(cmdBuffers) cmdBuffers;
	if (dmaQueue)
	{
		for (unsigned i = 0; i < size(cmdBuffers); i++)
		{
			auto &curCmdBuff = cmdBuffers[i];
			CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(curCmdBuff.allocator.GetAddressOf())));
			NameObjectF(curCmdBuff.allocator.Get(), L"DMA engine command allocator [%u]", i);
			CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, curCmdBuff.allocator.Get(), NULL, IID_PPV_ARGS(curCmdBuff.list.GetAddressOf())));
			NameObjectF(curCmdBuff.list.Get(), L"DMA engine command list [%u]", i);
		}
	}
	return cmdBuffers;
}

ComPtr<ID3D12Fence> DMA::Impl::CreateFence()
{
	ComPtr<ID3D12Fence> fence;
	if (dmaQueue)
	{
		CheckHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		NameObject(fence.Get(), L"DMA engine fence");
		if (atexit([] { WaitForGPU(lastBatchID); }))
			throw runtime_error("Fail to register GPU queue finalization for DMA engine.");
	}
	return fence;
}

void DMA::Upload2VRAM(const ComPtr<ID3D12Resource> &dst, const vector<D3D12_SUBRESOURCE_DATA> &src, LPCWSTR name)
{
	assert(dmaQueue);

	lock_guard lck(mtx);

	// use C++20 make_unique_default_init & monotonic_buffer_resource or allocation fusion
	const auto layouts = make_unique<D3D12_PLACED_SUBRESOURCE_FOOTPRINT []>(src.size());
	const auto numRows = make_unique<UINT []>(src.size());
	const auto rowSizes = make_unique<UINT64 []>(src.size());
	const auto suballocate = [&, dstDesc = dst->GetDesc()]
	{
		curBatchSuballocOffset = AlignSize<D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT>(curBatchSuballocOffset);
		UINT64 totalSize;
		device->GetCopyableFootprints(&dstDesc, 0, src.size(), curBatchSuballocOffset, layouts.get(), numRows.get(), rowSizes.get(), &totalSize);
		D3D12_RANGE allocation{ curBatchSuballocOffset };
		allocation.End = curBatchSuballocOffset += totalSize;
		return allocation;
	};

	auto allocRange = suballocate();
	auto *curBatch = uploadQueue + CurRingIdx();

	// start new batch and reallocate if overflow
	if (curBatchLen && curBatch->chunk && curBatchSuballocOffset > curBatch->chunk->GetDesc().Width)
	{
		FinishCurBatch();	// invalidates curBatch (updates ring idx)
		curBatch = uploadQueue + CurRingIdx();

		// suballocate again
		assert(!curBatchSuballocOffset);	// should be reseted in 'FinishCurBatch()' above
		allocRange = suballocate();
	}

	// prepare cmd list, including possible waiting for GPU progress
	const auto &cmdList = GetCmdList<true>();	// !: sync happens here

	/*
	good point for cleanup:
		after sync during cmd list preparation
		before possible chunk recreation
		while new batch scenario possible causing to CurRingIdx() wrapping to older batch (later batch marked as started making it no longer wrapped thus preventing its cleanup until later time)
	*/
	CleanupFinishedUploads();

	// handle initial case (chunk not created yet) and overflows due to huge textures not fitted into current chunk
	if (!curBatch->chunk || curBatchSuballocOffset > curBatch->chunk->GetDesc().Width)
	{
		assert(!curBatchLen);

		// defer chunk recreation after waiting for GPU in 'GetCmdList()' above (and releasing ref to old chunk in 'CleanupFinishedUploads()') to avoid RAM overuse - destroy old chunk before creating new one
		CheckHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(AlignSize<D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT>(max<UINT64>(defaultChunkSize, curBatchSuballocOffset))),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			NULL,	// clear value
			IID_PPV_ARGS(curBatch->chunk.ReleaseAndGetAddressOf())));
		NameObjectF(curBatch->chunk.Get(), L"DMA engine upload chunk [%u][%lu]", CurRingIdx(), curBatch->chunkVersion++);
	}

	// advance counter, mark batch as started (curBatchLen > 0)
	curBatchLen += src.size();
	assert(curBatchLen);

	// keep dst ref
	curBatch->outstandingRefs.push_back(dst);

	// copy data to upload chunk & record GPU commands for DMA engine
	std::byte *uploadPtr;
	CheckHR(curBatch->chunk->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void **>(&uploadPtr)));
	for (unsigned i = 0; i < src.size(); i++)
	{
		const auto &curLayout = layouts[i];
		{
			const D3D12_MEMCPY_DEST cpyDst = { uploadPtr + curLayout.Offset, curLayout.Footprint.RowPitch, curLayout.Footprint.RowPitch * numRows[i] };
			MemcpySubresource(&cpyDst, &src[i], rowSizes[i], numRows[i], curLayout.Footprint.Depth);
		}
		{
			const CD3DX12_TEXTURE_COPY_LOCATION cpyDst(dst.Get(), i), cpySrc(curBatch->chunk.Get(), curLayout);
			cmdList->CopyTextureRegion(&cpyDst, 0, 0, 0, &cpySrc, NULL);
		}
	}
	curBatch->chunk->Unmap(0, &allocRange);

	if (curBatchLen >= batchLenLimit || curBatchSuballocOffset >= curBatch->chunk->GetDesc().Width)
		FinishCurBatch();
}

void DMA::Sync()
{
	if (dmaQueue)
	{
		lock_guard lck(mtx);
		FlushPendingUploads(Texture::PendingLoadsCompleted());
		if (fence->GetCompletedValue() < lastBatchID)
		{
			/*
			it's called on beginning of a frame (waiting inserted at GFX queue) while end of the frame gets signaled by frame versioning fence
			hence last frame awaiting in frame versioning dtor will also wait for DMA fence to be reached on GPU
			any errors occurred during frame rendering currently must terminate process without cleanup (such errors unrecoverable yet)
			so last item in GFX queue during frame versioning dtor execution is last frame`s signal
			for more robust handling one may always insert additional signal (e.g. ~0) in frame versioning dtor (and wait for it)
			it would ensure proper waiting regardless of whether last GFX queue operation was frame finish
			*/
			CheckHR(gfxQueue->Wait(fence.Get(), lastBatchID));
		}
		CleanupFinishedUploads();
	}
}