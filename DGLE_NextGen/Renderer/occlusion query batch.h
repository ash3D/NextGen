/**
\author		Alexey Shaydurov aka ASH
\date		11.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <climits>
#include "tracked resource.h"

struct ID3D12QueryHeap;
struct ID3D12Resource;
struct ID3D12GraphicsCommandList1;

namespace Renderer::Impl::OcclusionCulling
{
	class QueryBatch
	{
		static Impl::TrackedResource<ID3D12QueryHeap> heapPool;
		static Impl::TrackedResource<ID3D12Resource> resultsPool;

	public:
		static constexpr unsigned long npos = ULONG_MAX;

	private:
		ID3D12QueryHeap *batchHeap;
		ID3D12Resource *batchResults;
		unsigned long count;
		mutable bool fresh;

	public:
		explicit QueryBatch(unsigned long count = 0);
		QueryBatch(QueryBatch &&) = default;
		QueryBatch &operator =(QueryBatch &&) = default;

	public:
		void Sync() const;
		void Start(unsigned long queryIdx, ID3D12GraphicsCommandList1 *target) const, Stop(unsigned long queryIdx, ID3D12GraphicsCommandList1 *target) const;
		void Set(unsigned long queryIdx, ID3D12GraphicsCommandList1 *target) const;
		void Resolve(ID3D12GraphicsCommandList1 *target) const, Finish(ID3D12GraphicsCommandList1 *target) const;
	};
}