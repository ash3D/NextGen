/**
\author		Alexey Shaydurov aka ASH
\date		02.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <utility>
#include <deque>
#include <shared_mutex>
#include <atomic>
#include "tracked resource.h"

struct ID3D12Resource;
struct D3D12_RESOURCE_DESC;

namespace Renderer::Impl::GPUStreamBuffer
{
	class AllocatorBase
	{
		struct RetiredFrame
		{
			UINT64 frameID;
			unsigned long freeEnd;
		};
		std::deque<RetiredFrame> retiredFrames;
		Impl::TrackedResource<ID3D12Resource> chunk;
		std::shared_mutex mtx;
		std::atomic<unsigned long> freeBegin = 0;
		unsigned long freeEnd = 0;
		unsigned long lockId = 0;
		bool freeRangeReversed = true;

	protected:
		AllocatorBase(unsigned long allocGranularity);
		AllocatorBase(AllocatorBase &) = delete;
		AllocatorBase &operator =(AllocatorBase &) = delete;
		~AllocatorBase() = default;

	private:
		void AllocateChunk(const D3D12_RESOURCE_DESC &chunkDesc);

	protected:
		std::pair<ID3D12Resource *, unsigned long> Allocate(unsigned long count, unsigned itemSize, unsigned long allocGranularity);

	public:
		void OnFrameFinish();
	};

	template<unsigned itemSize>
	class Allocator : public AllocatorBase
	{
		// use const instead of constexpr to allow out-of-class definition (to avoid dependency on D3D12 header here)
		static const unsigned long allocGranularity;

	public:
		Allocator();
		Allocator(Allocator &) = delete;
		Allocator &operator =(Allocator &) = delete;

	public:
		// result valid during current frame only
		std::pair<ID3D12Resource *, unsigned long> Allocate(unsigned long count);
	};

	template<unsigned itemSize>
	class CountedAllocatorWrapper
	{
		std::atomic<unsigned long> allocatedItemsCount;
		Allocator &allocator;

	public:
		explicit CountedAllocatorWrapper(Allocator<itemSize> &allocator) noexcept : allocator(allocator) {}
		CountedAllocatorWrapper(CountedAllocatorWrapper &&) = default;

	public:
		std::pair<ID3D12Resource *, unsigned long> Allocate(unsigned long count, unsigned itemSize, unsigned long allocGranularity);
		unsigned long GetAllocatedItemCount() const noexcept { return allocatedItemsCount.load(); }
	};
}