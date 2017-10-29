/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "GPU memory allocator.h"

template<class Resource>
std::pair<Resource, unsigned long> Renderer::GPUMemoryStreamedAllocator<Resource>::Allocate(unsigned long count)
{
	using namespace std;

	for (;;)
	{
		shared_lock<decltype(mtx)> sharedLock(mtx);
		const auto oldFreeBegin = freeBegin.fetch_add(count, memory_order_relaxed), newFreeBegin = oldFreeBegin + count;
		const bool wrap = newFreeBegin > chunkSize;
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
					const auto deficit = newFreeBegin - freeEnd;
					chunk = Resource::Allocate(chunkSize += (deficit + Resource::allocGranularity) / Resource::allocGranularity);
					retiredFrames.clear();
					freeBegin.store(freeEnd = 0, memory_order_relaxed);
					freeRangeReversed = true;
				}
				else // wrap
				{
					freeBegin.store(newFreeBegin, memory_order_relaxed);
					if (!retiredFrames.empty())
						retiredFrames.back().freeEnd = 0;	// remove bubble
					freeRangeReversed = false;
				}
				return { chunk, 0 };
			}
		}
		else
			return { chunk, oldFreeBegin };
	}
}

// NOTE: not thread-safe
template<class Resource>
void Renderer::GPUMemoryStreamedAllocator<Resource>::OnFrameFinish(UINT64 fenceValue)
{
	const UINT64 reachedFenceFalue;
	while (!retiredFrames.empty() && retiredFrames.front().fenceValue <= reachedFenceFalue)
	{
		if (retiredFrames.front().freeEnd < freeEnd)
			freeRangeReversed = true;
		freeEnd = retiredFrames.front().freeEnd;
		retiredFrames.pop();
	}
	if (retiredFrames.empty())
	{
		freeBegin.store(freeEnd = 0, std::memory_order_relaxed);
		freeRangeReversed = true;
	}

	const auto nextEnd = freeBegin.load(std::memory_order_relaxed);
	if (retiredFrames.empty() || nextEnd != retiredFrames.front().freeEnd)
		retiredFrames.push({ fenceValue, nextEnd });
}