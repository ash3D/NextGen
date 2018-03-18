/**
\author		Alexey Shaydurov aka ASH
\date		18.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "occlusion query batch.h"
#include "tracked resource.inl"
#include "align.h"
#ifdef _MSC_VER
#include <codecvt>
#include <locale>
#endif

using namespace std;
using namespace Renderer::Impl::OcclusionCulling;
using Microsoft::WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

template<typename T>
static void AtomicMax(atomic<T> &dst, T src)
{
	for (auto stored = dst.load(memory_order_relaxed); stored < src && !dst.compare_exchange_weak(stored, src, memory_order_relaxed););
}

#pragma region QueryBatchBase
unsigned long QueryBatchBase::heapPoolSize;

void QueryBatchBase::Setup(unsigned long count)
{
	try
	{
		if (this->count = count)
		{
			/*
			try to reduce pool realloc count by accounting for other render stages requests
			reset every frame is not necessary since currently there is no pool shrinking mechanism, it grwos monotonically
			*/
			static atomic<unsigned long> globalCountRequest;
			AtomicMax(globalCountRequest, count);

			static shared_mutex mtx;
			shared_lock<decltype(mtx)> sharedLock(mtx);

			/*
			check local request here
			do not consider global one - use it if actual reallocation is required
			it would reduce unnecessary pool reallocs
			*/
			if (count > heapPoolSize)
			{
				sharedLock.unlock();
				{
					lock_guard<decltype(mtx)> exclusiveLock(mtx);
					// other thread can have chance to replenish the pool, check again
					if (count > heapPoolSize)
					{
						static_assert(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT % sizeof(UINT64) == 0);
						// use global request for actual reallocation
						const auto newHeapPoolSize = AlignSize<D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT / sizeof(UINT64)>(globalCountRequest.load(memory_order_relaxed));

						static unsigned long version;

						const D3D12_QUERY_HEAP_DESC heapDesk = { D3D12_QUERY_HEAP_TYPE_OCCLUSION, newHeapPoolSize, 0 };
						CheckHR(device->CreateQueryHeap(&heapDesk, IID_PPV_ARGS(heapPool.ReleaseAndGetAddressOf())));
						heapPoolSize = newHeapPoolSize;	// after creation for sake of exception safety
						NameObjectF(heapPool.Get(), L"occlusion query heap [%lu]", version);
					}
				}
				sharedLock.lock();
			}

			// tracked->untracked copy here is safe since it holds for single frame only\
			untracked used to eliminate unnecessary overhead of retirement every frame
			batchHeap = heapPool.Get();

			// setup derived
			FinalSetup();
		}
	}
	catch (...)
	{
		// basic exception safety guarantee
		count = 0;
		throw;
	}
}

void QueryBatchBase::Start(ID3D12GraphicsCommandList1 *cmdList, unsigned long queryIdx) const
{
	assert(queryIdx < count);
	cmdList->BeginQuery(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIdx);
}

void QueryBatchBase::Stop(ID3D12GraphicsCommandList1 *cmdList, unsigned long queryIdx) const
{
	assert(queryIdx < count);
	cmdList->EndQuery(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIdx);
}

void QueryBatchBase::Set(ID3D12GraphicsCommandList1 *cmdList, unsigned long queryIdx, ID3D12Resource *batchResults, bool visible) const
{
	assert(queryIdx == npos || queryIdx < count);
	cmdList->SetPredication(queryIdx == npos ? NULL : batchResults, queryIdx == npos ? 0 : queryIdx * sizeof(UINT64), visible ? D3D12_PREDICATION_OP_EQUAL_ZERO : D3D12_PREDICATION_OP_NOT_EQUAL_ZERO);
}
#pragma endregion

#pragma region QueryBatch<false>
QueryBatch<true>::QueryBatch(const std::string &name) : name(name) {}
QueryBatch<true>::~QueryBatch() = default;

void QueryBatch<false>::FinalSetup()
{
	// same approach for pool allocation as for query heap in QueryBatchBase
	const unsigned long requiredSize = count * sizeof(UINT64);
	static atomic<unsigned long> globalSizeRequest;
	AtomicMax(globalSizeRequest, requiredSize);

	static shared_mutex mtx;
	shared_lock<decltype(mtx)> sharedLock(mtx);

	if (!resultsPool || requiredSize > resultsPool->GetDesc().Width)
	{
		sharedLock.unlock();
		{
			lock_guard<decltype(mtx)> exclusiveLock(mtx);
			// other thread can have chance to replenish the pool, check again
			if (!resultsPool || requiredSize > resultsPool->GetDesc().Width)
			{
				// use global request for actual reallocation
				const auto newResultsSize = AlignSize<D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT>(globalSizeRequest.load(memory_order_relaxed));

				static unsigned long version;

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

	batchResults = resultsPool.Get();
}

// not thread-safe, must be called sequentially in render stage order
void QueryBatch<false>::Sync() const
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

void QueryBatch<false>::Resolve(ID3D12GraphicsCommandList1 *cmdList) const
{
	if (count)
	{
		if (!fresh)
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY));
		cmdList->ResolveQueryData(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, count, batchResults, 0);
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION));
	}
}

void QueryBatch<false>::Finish(ID3D12GraphicsCommandList1 *cmdList) const
{
	if (count)
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY));
}
#pragma endregion

#pragma region QueryBatch<true>
void QueryBatch<true>::FinalSetup()
{
	if (const unsigned long requiredSize = count * sizeof(UINT64); !batchResults || batchResults->GetDesc().Width < requiredSize)
	{
		const CD3DX12_RESOURCE_DESC resultsDesc(
			D3D12_RESOURCE_DIMENSION_BUFFER,
			D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			requiredSize,
			1,		// height
			1,		// depth
			1,		// mips
			DXGI_FORMAT_UNKNOWN,
			1, 0,	// MSAA
			D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			D3D12_RESOURCE_FLAG_NONE);
		CheckHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&resultsDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			NULL,	// clear value
			IID_PPV_ARGS(batchResults.ReleaseAndGetAddressOf())));
#ifdef _MSC_VER
		wstring_convert<codecvt_utf8<WCHAR>> converter;
		NameObjectF(batchResults.Get(), L"occlusion query results for %ls [%lu]", converter.from_bytes(name).c_str(), version++);
#else
		NameObjectF(batchResults.Get(), L"occlusion query results for %s [%lu]", name.c_str(), version++);
#endif
	}
}

void QueryBatch<true>::Resolve(ID3D12GraphicsCommandList1 *cmdList, long int usage) const
{
	if (count)
	{
		// rely on implicit state transition here
		cmdList->ResolveQueryData(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, count, batchResults.Get(), 0);
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(batchResults.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION | D3D12_RESOURCE_STATES(usage)));
	}
}

UINT64 QueryBatch<true>::GetGPUPtr() const
{
	return batchResults->GetGPUVirtualAddress();
}
#pragma endregion