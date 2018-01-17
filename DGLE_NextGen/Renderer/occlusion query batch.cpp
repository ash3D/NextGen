/**
\author		Alexey Shaydurov aka ASH
\date		17.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "occlusion query batch.h"
#include "tracked resource.inl"
#include "align.h"

using namespace std;
using namespace Renderer::Impl::OcclusionCulling;
using Microsoft::WRL::ComPtr;

QueryBatch::QueryBatch(unsigned long count) : count(count)
{
	extern ComPtr<ID3D12Device2> device;
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

	if (const unsigned long requiredSize = count * sizeof(UINT64))
	{
		/*
			try to reduce pool realloc count by accounting for other render stages requests
			reset every frame is not necessary since currently there is no pool shrinking mechanism, it grwos monotonically
		*/
		static atomic<unsigned long> globalSizeRequest;
		// perform atomic max
		for (auto stored = globalSizeRequest.load(memory_order_relaxed); stored < requiredSize && !globalSizeRequest.compare_exchange_weak(stored, requiredSize, memory_order_relaxed););

		static shared_mutex mtx;
		shared_lock<decltype(mtx)> sharedLock(mtx);

		/*
			check local request here
			do not consider global one - use it if actual reallocation is required
			it would reduce unnecessary pool reallocs
		*/
		if (!resultsPool || requiredSize > resultsPool->GetDesc().Width)
		{
			sharedLock.unlock();
			{
				lock_guard<decltype(mtx)> exclusiveLock(mtx);
				// other thread can have chance to replenish the pool, check again
				if (!resultsPool || requiredSize > resultsPool->GetDesc().Width)
				{
					static_assert(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT % sizeof(UINT64) == 0);
					// use global request for actual reallocation
					const auto newResultsSize = AlignSize<D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT>(globalSizeRequest.load(memory_order_relaxed));

					static unsigned long version;

					const D3D12_QUERY_HEAP_DESC heapDesk = { D3D12_QUERY_HEAP_TYPE_OCCLUSION, newResultsSize / sizeof(UINT64), 0 };
					const CD3DX12_RESOURCE_DESC resultsDesc(
						D3D12_RESOURCE_DIMENSION_BUFFER,
						D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
						newResultsSize,
						1,		// height
						1,		// depth
						1,		// mips
						DXGI_FORMAT_UNKNOWN,
						1, 0,	// MSAA
						D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
						D3D12_RESOURCE_FLAG_NONE);
					CheckHR(device->CreateQueryHeap(&heapDesk, IID_PPV_ARGS(heapPool.ReleaseAndGetAddressOf())));
					NameObjectF(heapPool.Get(), L"occlusion query heap [%lu]", version);
					CheckHR(device->CreateCommittedResource(
						&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
						D3D12_HEAP_FLAG_NONE,
						&resultsDesc,
						D3D12_RESOURCE_STATE_COPY_DEST,
						NULL,	// clear value
						IID_PPV_ARGS(resultsPool.ReleaseAndGetAddressOf())));
					NameObjectF(resultsPool.Get(), L"occlusion query results [%lu]", version++);
				}
			}
			sharedLock.lock();
		}

		// tracked->untracked copy here is safe since it holds for single frame only\
		untracked used to eliminate unnecessary overhead of retirement every frame
		batchHeap = heapPool.Get();
		batchResults = resultsPool.Get();
	}
}

// not thread-safe, must be called sequentially in render stage order
void QueryBatch::Sync() const
{
	if (count)
	{
		// {89D5F2E9-3173-4883-8C86-2AC2C62A08C5}
		static const GUID reusedBatchGUID = 
		{ 0x89d5f2e9, 0x3173, 0x4883, { 0x8c, 0x86, 0x2a, 0xc2, 0xc6, 0x2a, 0x8, 0xc5 } };

		UINT size = 0;
		if (const HRESULT hr = batchHeap->GetPrivateData(reusedBatchGUID, &size, NULL); fresh = hr == DXGI_ERROR_NOT_FOUND)
			CheckHR(batchHeap->SetPrivateData(reusedBatchGUID, 0, batchHeap));	// pass some non-NULL ptr to register reusedBatchGUID
		else
			CheckHR(hr);
	}
}

void QueryBatch::Start(unsigned long queryIdx, ID3D12GraphicsCommandList1 *cmdList) const
{
	assert(queryIdx < count);
	cmdList->BeginQuery(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIdx);
}

void QueryBatch::Stop(unsigned long queryIdx, ID3D12GraphicsCommandList1 *cmdList) const
{
	assert(queryIdx < count);
	cmdList->EndQuery(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIdx);
}

void QueryBatch::Set(unsigned long queryIdx, ID3D12GraphicsCommandList1 *cmdList) const
{
	assert(queryIdx == npos || queryIdx < count);
	cmdList->SetPredication(queryIdx == npos ? NULL : batchResults, queryIdx == npos ? 0 : queryIdx * sizeof(UINT64), D3D12_PREDICATION_OP_EQUAL_ZERO);
}

void QueryBatch::Resolve(ID3D12GraphicsCommandList1 *cmdList) const
{
	if (count)
	{
		if (!fresh)
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY));
		cmdList->ResolveQueryData(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, count, batchResults, 0);
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION));
	}
}

void QueryBatch::Finish(ID3D12GraphicsCommandList1 *cmdList) const
{
	if (count)
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY));
}