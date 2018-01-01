/**
\author		Alexey Shaydurov aka ASH
\date		01.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "GPU stream buffer allocator.h"
#include "tracked resource.inl"
#include "frame versioning.h"

using namespace std;
using namespace Renderer::Impl;

void GPUStreamBufferAllocatorBase::AllocateChunk(const D3D12_RESOURCE_DESC &chunkDesc)
{
	extern Microsoft::WRL::ComPtr<ID3D12Device2> device;

	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&chunkDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,	// clear value
		IID_PPV_ARGS(chunk.ReleaseAndGetAddressOf())));
}

pair<ID3D12Resource *, unsigned long> GPUStreamBufferAllocatorBase::Allocate(unsigned long count, unsigned itemSize, unsigned long allocGranularity)
{
	for (;;)
	{
		shared_lock<decltype(mtx)> sharedLock(mtx);
		auto chunkDesc = chunk->GetDesc();
		const auto oldFreeBegin = freeBegin.fetch_add(count, memory_order_relaxed);
		auto newFreeBegin = oldFreeBegin + count;
		const bool wrap = newFreeBegin * itemSize > chunkDesc.Width;
		if (wrap)
			newFreeBegin = count;
		const bool overflow = freeRangeReversed == wrap ? newFreeBegin > freeEnd : wrap;
		if (wrap || overflow)
		{
			const unsigned savedLockId = lockId;
			sharedLock.unlock();
			lock_guard<decltype(mtx)> exclusiveLock(mtx);
			if (lockId != savedLockId)
				continue;	// try again
			else
			{
				lockId++;
				if (overflow)
				{
					const auto deficit = (newFreeBegin - freeEnd) * itemSize;
					chunkDesc.Width += (deficit + allocGranularity - 1) / allocGranularity;
					AllocateChunk(chunkDesc);
					retiredFrames.clear();
					freeBegin.store(freeEnd = 0, memory_order_relaxed);
					freeRangeReversed = true;
				}
				else // wrap
				{
					assert(wrap);
					freeBegin.store(newFreeBegin, memory_order_relaxed);
					if (!retiredFrames.empty())
						retiredFrames.back().freeEnd = 0;	// remove bubble
					freeRangeReversed = false;
				}
				return { chunk.Get(), 0 };
			}
		}
		else
			return { chunk.Get(), oldFreeBegin };
	}
}

// NOTE: not thread-safe
void GPUStreamBufferAllocatorBase::OnFrameFinish()
{
	for (const UINT64 completedFrameID = globalFrameVersioning->GetCompletedFrameID(); !retiredFrames.empty() && retiredFrames.front().frameID <= completedFrameID; retiredFrames.pop_front())
	{
		if (retiredFrames.front().freeEnd < freeEnd)
			freeRangeReversed = true;
		freeEnd = retiredFrames.front().freeEnd;
	}
	if (retiredFrames.empty())
	{
		freeBegin.store(freeEnd = 0, memory_order_relaxed);
		freeRangeReversed = true;
	}

	if (const auto nextEnd = freeBegin.load(memory_order_relaxed); retiredFrames.empty() || nextEnd != retiredFrames.front().freeEnd)
		retiredFrames.push_back({ globalFrameVersioning->GetCurFrameID(), nextEnd });
}