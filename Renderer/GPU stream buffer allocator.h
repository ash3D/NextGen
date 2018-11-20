#pragma once

#include <utility>
#include <deque>
#include <shared_mutex>
#include <atomic>
#include "../tracked resource.h"

struct ID3D12Resource;
struct D3D12_RESOURCE_DESC;

namespace Renderer::Impl::GPUStreamBuffer
{
	class AllocatorBase
	{
		struct RetiredFrame
		{
			UINT64 frameID;
			unsigned long usedEnd;
		};
		std::deque<RetiredFrame> retiredFrames;
		Impl::TrackedResource<ID3D12Resource> chunk;
		std::shared_mutex mtx;
		std::atomic<unsigned long> freeBegin = 0;
		unsigned long freeEnd = 0;
		unsigned long lockStamp = 0, chunkVersion = 0;
		bool freeRangeReversed = true;

	protected:
		AllocatorBase(unsigned long allocGranularity, LPCWSTR resourceName);
		AllocatorBase(AllocatorBase &) = delete;
		AllocatorBase &operator =(AllocatorBase &) = delete;
		~AllocatorBase() = default;

	private:
		void AllocateChunk(const D3D12_RESOURCE_DESC &chunkDesc, LPCWSTR resourceName);

	protected:
		std::pair<ID3D12Resource *, unsigned long> Allocate(unsigned long count, unsigned itemSize, unsigned long allocGranularity, LPCWSTR resourceName);

	public:
		void OnFrameFinish();
	};

	template<unsigned itemSize, LPCWSTR resourceName>
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
}