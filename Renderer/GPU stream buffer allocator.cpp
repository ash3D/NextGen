#include "stdafx.h"
#include "GPU stream buffer allocator.h"
#include "tracked resource.inl"
#include "frame versioning.h"

using namespace std;
using namespace Renderer::Impl::GPUStreamBuffer;

void AllocatorBase::AllocateChunk(const D3D12_RESOURCE_DESC &chunkDesc, LPCWSTR resourceName)
{
	extern Microsoft::WRL::ComPtr<ID3D12Device4> device;
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

	const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	CheckHR(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&chunkDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,	// clear value
		IID_PPV_ARGS(chunk.ReleaseAndGetAddressOf())));
	NameObjectF(chunk.Get(), L"%ls (chunk[%lu])", resourceName, chunkVersion++);
}

pair<ID3D12Resource *, unsigned long> AllocatorBase::Allocate(unsigned long count, unsigned itemSize, unsigned long allocGranularity, LPCWSTR resourceName)
{
start:
	shared_lock sharedLock(mtx);
	auto chunkDesc = chunk->GetDesc();
	const auto oldFreeBegin = freeBegin.fetch_add(count, memory_order_relaxed);
	auto newFreeBegin = oldFreeBegin + count;
	const bool wrap = newFreeBegin * itemSize > chunkDesc.Width;
	if (wrap)
		newFreeBegin = count;
	const bool overflow = freeRangeReversed == wrap ? newFreeBegin > freeEnd : wrap;
	if (wrap || overflow)
	{
		const unsigned savedLockStamp = lockStamp;
		sharedLock.unlock();
		lock_guard exclusiveLock(mtx);
		if (lockStamp != savedLockStamp)
			goto start;	// try again
		else
		{
			lockStamp++;
			if (overflow)
			{
				auto deficit = (newFreeBegin - freeEnd) * itemSize;
				deficit += allocGranularity - 1;
				deficit -= deficit % allocGranularity;
				chunkDesc.Width += deficit;
				AllocateChunk(chunkDesc, resourceName);
				retiredFrames.clear();
				freeBegin.store(count, memory_order_relaxed);
				freeEnd = 0;
				freeRangeReversed = true;
			}
			else // wrap
			{
				assert(wrap);
				freeBegin.store(newFreeBegin, memory_order_relaxed);
				if (!retiredFrames.empty())
					retiredFrames.back().usedEnd = 0;	// remove bubble
				freeRangeReversed = false;
			}
			return { chunk.Get(), 0 };
		}
	}
	else
		return { chunk.Get(), oldFreeBegin };
}

// NOTE: not thread-safe
void AllocatorBase::OnFrameFinish()
{
	// register current frame allocations
	if (const auto curFrameUsedEnd = freeBegin.load(memory_order_relaxed); retiredFrames.empty() || curFrameUsedEnd != retiredFrames.front().usedEnd)
		retiredFrames.push_back({ globalFrameVersioning->GetCurFrameID(), curFrameUsedEnd });

	// free completed frames
	for (const UINT64 completedFrameID = globalFrameVersioning->GetCompletedFrameID(); !retiredFrames.empty() && retiredFrames.front().frameID <= completedFrameID; retiredFrames.pop_front())
	{
		if (retiredFrames.front().usedEnd < freeEnd)
			freeRangeReversed = true;
		freeEnd = retiredFrames.front().usedEnd;
	}

	// reset start allocation position if chunk is completely free - it can reduce the chance of overflow when large continuous ranges being allocated
	if (retiredFrames.empty())
	{
		freeBegin.store(freeEnd = 0, memory_order_relaxed);
		freeRangeReversed = true;
	}
}