/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"

namespace Renderer
{
	template<class Resource>
	class GPUMemoryStreamedAllocator
	{
		struct RetiredFrame
		{
			UINT64 fenceValue;
			unsigned long freeEnd;
		};
		std::queue<RetiredFrame> retiredFrames;
		Resource chunk = Resource::Allocate(Resource::allocGranularity);
		std::shared_mutex mtx;
		std::atomic<unsigned long> freeBegin = 0;
		unsigned long freeEnd = 0, chunkSize = Resource::allocGranularity;
		unsigned long lockId = 0;
		bool freeRangeReversed = true;

	public:
		GPUMemoryStreamedAllocator(GPUMemoryStreamedAllocator &&) = default;
		GPUMemoryStreamedAllocator &operator =(GPUMemoryStreamedAllocator &&) = default;

	public:
		std::pair<Resource, unsigned long> Allocate(unsigned long count);
		void OnFrameFinish(UINT64 fenceValue);
	};
}

#include "GPU memory allocator.inl"