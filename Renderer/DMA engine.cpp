#include "stdafx.h"
#include "DMA engine.h"
#include "texture.hh"
#include "event handle.h"
#include "align.h"

#define DEFER_UPLOADS_SUBMISSION 1

using namespace std;
using namespace Renderer;
using namespace DMA::Impl;
using DMA::WRL::ComPtr;

extern ComPtr<ID3D12Device4> device;
extern ComPtr<ID3D12CommandQueue> gfxQueue, dmaQueue;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

static constexpr auto defaultChunkSize = /*128ul*/32ul * 1024ul * 1024ul;
static constexpr auto batchLenLimit = /*1024u*/256u;

// {B21BD34F-2B98-46C2-BA23-8ABD09392251}
static const GUID uploadBatchMarkerGUID =
{ 0xb21bd34f, 0x2b98, 0x46c2, { 0xba, 0x23, 0x8a, 0xbd, 0x9, 0x39, 0x22, 0x51 } };

static UINT64 lastBatchID, consumedBatchID/*batch containing consumed resources (e.g. textures binded to materials) => have to be uploaded before frame starts*/;
static UINT64 curBatchSuballocOffset;
static UINT curBatchLen;

static Impl::EventHandle fenceEvent;
static recursive_mutex mtx;

struct Batch
{
	ComPtr<ID3D12Resource> chunk;
	unsigned long chunkVersion;
	vector<ComPtr<ID3D12Resource>> outstandingRefs;
	vector<future<void>> deferredOps;
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

	// wait for differed ops completion
	for_each(curBatch.deferredOps.begin(), curBatch.deferredOps.end(), mem_fn(&decltype(curBatch.deferredOps)::value_type::get));
	curBatch.deferredOps.clear();
	
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

void DMA::Upload2VRAM(const ComPtr<ID3D12Resource> &dst, span<const D3D12_SUBRESOURCE_DATA> src, LPCWSTR name)
{
	assert(dmaQueue);

	unique_lock lck(mtx);

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

	// record GPU commands for DMA engine
	for (unsigned i = 0; i < src.size(); i++)
	{
		const CD3DX12_TEXTURE_COPY_LOCATION cpyDst(dst.Get(), i), cpySrc(curBatch->chunk.Get(), layouts[i]);
		cmdList->CopyTextureRegion(&cpyDst, 0, 0, 0, &cpySrc, NULL);
	}

	/*
	defer memcpys to perform after mutex unlock to enable multithreaded copy
	can benefit on AMD Zen 2 CPUs:
		benchmarks shows half RAM write bandwidth on 1 CCD CPUs while 2 CCD doesn't have such slowdown
		one assumption is Infinity Fabric limitations
		if this assumption is true then singlethreaded writes can't saturate RAM bus on CPUs comprising of 2 CCD (both CCD have to do writes to fully utilize RAM bus)
		while copy ops do not subject to this performance issue on Zen 2, this workload (copy to GPU accessible buffer) can essentially be write op:
			after textures have been read from file to writeback (cached) RAM region they can reside in cache (especially considering large L3 cache on Zen 2)
			then texture data have to be written to GPU accessible writecombine (thus uncached, i.e. it write to RAM, not to cache) region
			so copy cache -> RAM is essentially write from RAM point of view => multithreading can help on 2 CCDs Zen2
	*/
	packaged_task<void()> deferred([&, curBatch]
		{
			// copy data to upload chunk
			std::byte *uploadPtr;
			CheckHR(curBatch->chunk->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void **>(&uploadPtr)));
			for (unsigned i = 0; i < src.size(); i++)
			{
				const auto &curLayout = layouts[i];
				const D3D12_MEMCPY_DEST cpyDst = { uploadPtr + curLayout.Offset, curLayout.Footprint.RowPitch, curLayout.Footprint.RowPitch * numRows[i] };
				MemcpySubresource(&cpyDst, &src[i], rowSizes[i], numRows[i], curLayout.Footprint.Depth);
			}
			curBatch->chunk->Unmap(0, &allocRange);

			// set upload batch marker
			const UINT64 uploadBatchMarker = lastBatchID + 1/*cur batch*/;
			CheckHR(dst->SetPrivateData(uploadBatchMarkerGUID, sizeof uploadBatchMarker, &uploadBatchMarker));
		});

	if (curBatchLen >= batchLenLimit || curBatchSuballocOffset >= curBatch->chunk->GetDesc().Width)
	{
		deferred();
		FinishCurBatch();
	}
	else
	{
		curBatch->deferredOps.push_back(deferred.get_future());
		lck.unlock();
		deferred();
	}
}

// have to be called before 'Sync()'
void DMA::TrackUsage(ID3D12Resource *res)
{
	UINT64 uploadBatchMarker;
	UINT markerSize = sizeof uploadBatchMarker;
	if (const HRESULT hr = res->GetPrivateData(uploadBatchMarkerGUID, &markerSize, &uploadBatchMarker); hr != DXGI_ERROR_NOT_FOUND)
	{
		CheckHR(hr);
		assert(markerSize == sizeof uploadBatchMarker);
		consumedBatchID = max(consumedBatchID, uploadBatchMarker);
	}
	// do nothing if DXGI_ERROR_NOT_FOUND - it means that resource placed in sys RAM and wasn't uploaded via DMA engine
}

void DMA::Sync()
{
	if (dmaQueue)
	{
		lock_guard lck(mtx);
#		if DEFER_UPLOADS_SUBMISSION
		if (consumedBatchID > lastBatchID)
#		endif
			FlushPendingUploads(Texture::PendingLoadsCompleted());
		if (fence->GetCompletedValue() < consumedBatchID)
		{
			/*
			it's called on beginning of a frame (waiting inserted at GFX queue) while end of the frame gets signaled by frame versioning fence
			hence last frame awaiting in frame versioning dtor will also wait for DMA fence to be reached on GPU
			any errors occurred during frame rendering currently must terminate process without cleanup (such errors unrecoverable yet)
			so last item in GFX queue during frame versioning dtor execution is last frame`s signal
			for more robust handling one may always insert additional signal (e.g. ~0) in frame versioning dtor (and wait for it)
			it would ensure proper waiting regardless of whether last GFX queue operation was frame finish
			*/
			CheckHR(gfxQueue->Wait(fence.Get(), consumedBatchID));
		}
		CleanupFinishedUploads();
	}
}