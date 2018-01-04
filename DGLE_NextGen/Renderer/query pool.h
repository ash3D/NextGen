/**
\author		Alexey Shaydurov aka ASH
\date		04.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <limits>
#include "tracked resource.h"

struct ID3D12QueryHeap;
struct ID3D12Resource;
struct ID3D12GraphicsCommandList1;

namespace Renderer::Impl::QueryPool
{
	struct OcclusionQueryHandle
	{
		friend class OcclusionQueryBatch;

	private:
		UINT idx = std::numeric_limits<decltype(idx)>::max();

	public:
		operator bool() const noexcept { return idx != std::numeric_limits<decltype(idx)>::max(); }
		OcclusionQueryHandle &operator ++() noexcept { return ++idx, *this; }
		// use C++20 <=>
		bool operator ==(OcclusionQueryHandle src) const noexcept { return idx == src.idx; }
	};

	class OcclusionQueryBatch
	{
		static Impl::TrackedResource<ID3D12QueryHeap> heapPool;
		static Impl::TrackedResource<ID3D12Resource> resultsPool;

	private:
		ID3D12QueryHeap *batchHeap;
		ID3D12Resource *batchResults;
		unsigned long count;
		bool fresh;

	public:
		explicit OcclusionQueryBatch(unsigned long count);
		OcclusionQueryBatch(OcclusionQueryBatch &&) = default;
		OcclusionQueryBatch &operator =(OcclusionQueryBatch &&) = default;

	public:
		void Start(OcclusionQueryHandle queryHandle, ID3D12GraphicsCommandList1 *target), Stop(OcclusionQueryHandle queryHandle, ID3D12GraphicsCommandList1 *target);
		void Set(OcclusionQueryHandle queryHandle, ID3D12GraphicsCommandList1 *target);
		void Resolve(ID3D12GraphicsCommandList1 *target), Finish(ID3D12GraphicsCommandList1 *target);
	};
}