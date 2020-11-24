#include "stdafx.h"
#include "occlusion query batch.h"
#include "tracked resource.inl"
#include "cmdlist pool.inl"
#include "align.h"
#ifdef _MSC_VER
#include <codecvt>
#include <locale>
#endif

using namespace std;
using namespace Renderer::Impl;
using namespace OcclusionCulling;
using WRL::ComPtr;

extern ComPtr<ID3D12Device4> device;
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
			reset every frame is not necessary since currently there is no pool shrinking mechanism, it grows monotonically
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
					lock_guard exclusiveLock(mtx);
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

void QueryBatchBase::Start(ID3D12GraphicsCommandList4 *cmdList, unsigned long queryIdx) const
{
	assert(queryIdx < count);
	cmdList->BeginQuery(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIdx);
}

void QueryBatchBase::Stop(ID3D12GraphicsCommandList4 *cmdList, unsigned long queryIdx) const
{
	assert(queryIdx < count);
	cmdList->EndQuery(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, queryIdx);
}

void QueryBatchBase::Set(ID3D12GraphicsCommandList4 *cmdList, unsigned long queryIdx, ID3D12Resource *batchResults, bool visible, unsigned long offset) const
{
	assert(queryIdx == npos || queryIdx < count);
	cmdList->SetPredication(queryIdx == npos ? NULL : batchResults, queryIdx == npos ? 0 : queryIdx * sizeof(UINT64) + offset, visible ? D3D12_PREDICATION_OP_EQUAL_ZERO : D3D12_PREDICATION_OP_NOT_EQUAL_ZERO);
}
#pragma endregion

#pragma region TRANSIENT
void QueryBatch<TRANSIENT>::FinalSetup()
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
			lock_guard exclusiveLock(mtx);
			// other thread can have chance to replenish the pool, check again
			if (!resultsPool || requiredSize > resultsPool->GetDesc().Width)
			{
				// use global request for actual reallocation
				const auto newResultsSize = AlignSize<D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT>(globalSizeRequest.load(memory_order_relaxed));

				static unsigned long version;

				const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
				const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(newResultsSize);
				CheckHR(device->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&bufferDesc,
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
void QueryBatch<TRANSIENT>::Sync() const
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

void QueryBatch<TRANSIENT>::Resolve(CmdListPool::CmdList &cmdList, bool reuse) const
{
	if (count)
	{
		if (!fresh || reuse)
		{
			const auto flags = reuse ? D3D12_RESOURCE_BARRIER_FLAG_NONE : D3D12_RESOURCE_BARRIER_FLAG_END_ONLY/*!fresh*/;
			cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, flags));
		}
		cmdList.FlushBarriers();
		cmdList->ResolveQueryData(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, count, batchResults, 0);
		cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION));
	}
}

void QueryBatch<TRANSIENT>::Finish(CmdListPool::CmdList &cmdList) const
{
	if (count)
		cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(batchResults, D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY));
}
#pragma endregion

#pragma region PERSISTENT
QueryBatch<PERSISTENT>::QueryBatch(const string &name) : name(name) {}
QueryBatch<PERSISTENT>::~QueryBatch() = default;

void QueryBatch<PERSISTENT>::FinalSetup()
{
	if (const unsigned long requiredSize = count * sizeof(UINT64); !batchResults || batchResults->GetDesc().Width < requiredSize)
	{
		const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
		CheckHR(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			NULL,	// clear value
			IID_PPV_ARGS(batchResults.ReleaseAndGetAddressOf())));
#ifdef _MSC_VER
		wstring_convert<codecvt_utf8<WCHAR>> converter;
		NameObjectF(batchResults.Get(), L"occlusion query results for %ls [%lu] (query batch %p)", converter.from_bytes(name).c_str(), version++, this);
#else
		NameObjectF(batchResults.Get(), L"occlusion query results for %s [%lu] (query batch %p)", name.c_str(), version++, this);
#endif
	}
}

void QueryBatch<PERSISTENT>::Resolve(CmdListPool::CmdList &cmdList, long int usage) const
{
	if (count)
	{
		// rely on implicit state transition here
		cmdList->ResolveQueryData(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, count, batchResults.Get(), 0);
		cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(batchResults.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION | D3D12_RESOURCE_STATES(usage)));
	}
}

UINT64 QueryBatch<PERSISTENT>::GetGPUPtr() const
{
	return batchResults->GetGPUVirtualAddress();
}
#pragma endregion

#pragma region DUAL
QueryBatch<DUAL>::QueryBatch(LPCWSTR name, long int firstUsage, long int secondUsage, unsigned alignment) :
	name(name), usages{ D3D12_RESOURCE_STATE_PREDICATION | firstUsage, D3D12_RESOURCE_STATE_PREDICATION | secondUsage },
	alignment(max<unsigned>(alignment, sizeof(UINT64)))	// max can be used here instead of lcm since alignment is forced to be power of 2
{
	assert(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT % this->alignment == 0);
}

QueryBatch<DUAL>::~QueryBatch() = default;

void QueryBatch<DUAL>::FinalSetup()
{
	if (const unsigned long moietySize = count * sizeof(UINT64), requiredSize = AlignSize(moietySize, alignment) + moietySize; !batchResults || batchResults->GetDesc().Width < requiredSize)
	{
		const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
		CheckHR(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			NULL,	// clear value
			IID_PPV_ARGS(batchResults.ReleaseAndGetAddressOf())));
		NameObjectF(batchResults.Get(), L"occlusion query results for %ls [%lu] (query batch %p)", name, version++, this);
	}
}

void QueryBatch<DUAL>::Resolve(CmdListPool::CmdList &cmdList, bool second) const
{
	if (count)
	{
		// rely on implicit state transition on first resolve
		if (second)
			cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(batchResults.Get(), D3D12_RESOURCE_STATES(usages[0]), D3D12_RESOURCE_STATE_COPY_DEST));
		cmdList.FlushBarriers();
		cmdList->ResolveQueryData(batchHeap, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, count, batchResults.Get(), Offset(second));
		cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(batchResults.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATES(usages[second])));
	}
}

UINT64 QueryBatch<DUAL>::GetGPUPtr(bool second) const
{
	return batchResults->GetGPUVirtualAddress() + Offset(second);
}

unsigned long QueryBatch<DUAL>::Offset(bool second) const noexcept
{
	return AlignSize(count * sizeof(UINT64), alignment) * second;
}
#pragma endregion