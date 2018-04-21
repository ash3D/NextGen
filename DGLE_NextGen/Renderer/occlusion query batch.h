/**
\author		Alexey Shaydurov aka ASH
\date		21.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <climits>
#include <string>
#include "tracked resource.h"

struct ID3D12QueryHeap;
struct ID3D12Resource;
struct ID3D12GraphicsCommandList2;

namespace Renderer::Impl::OcclusionCulling
{
	enum QueryBatchType
	{
		TRANSIENT,
		PERSISTENT,
		DUAL,
	};

	class QueryBatchBase
	{
		static Impl::TrackedResource<ID3D12QueryHeap> heapPool;
		static unsigned long heapPoolSize;

	public:
		static constexpr unsigned long npos = ULONG_MAX;

	protected:
		ID3D12QueryHeap *batchHeap;
		unsigned long count = 0;

	protected:
		QueryBatchBase() = default;
		QueryBatchBase(QueryBatchBase &) = delete;
		QueryBatchBase &operator =(QueryBatchBase &) = delete;
		~QueryBatchBase() = default;

	private:
		virtual void FinalSetup() = 0;

	public:
		void Setup(unsigned long count);

	protected:
		void Set(ID3D12GraphicsCommandList2 *target, unsigned long queryIdx, ID3D12Resource *batchResults, bool visible, unsigned long offset = 0ul) const;

	public:
		void Start(ID3D12GraphicsCommandList2 *target, unsigned long queryIdx) const, Stop(ID3D12GraphicsCommandList2 *target, unsigned long queryIdx) const;
	};

	template<QueryBatchType>
	class QueryBatch;

	template<>
	class QueryBatch<TRANSIENT> final : public QueryBatchBase
	{
		static Impl::TrackedResource<ID3D12Resource> resultsPool;

	private:
		ID3D12Resource *batchResults;
		mutable bool fresh;

	private:
		void FinalSetup() override;

	public:
		void Sync() const;
		void Set(ID3D12GraphicsCommandList2 *target, unsigned long queryIdx, bool visible = true) const { QueryBatchBase::Set(target, queryIdx, batchResults, visible); }
		void Resolve(ID3D12GraphicsCommandList2 *target, bool reuse = false) const, Finish(ID3D12GraphicsCommandList2 *target) const;
	};

	template<>
	class QueryBatch<PERSISTENT> final : public QueryBatchBase
	{
		Impl::TrackedResource<ID3D12Resource> batchResults;
		const std::string &name;
		unsigned long version = 0;	// increments within lifetime of batch object, resets to 0 on new object construction

	public:
		// holds reference to the string, thus it's lifetime must be appropriate; a better and safer approach would be using immutable Name class
		QueryBatch(const std::string &name);
		~QueryBatch();

	private:
		void FinalSetup() override;

	public:
		void Set(ID3D12GraphicsCommandList2 *target, unsigned long queryIdx, bool visible = true) const { QueryBatchBase::Set(target, queryIdx, batchResults.Get(), visible); }
		void Resolve(ID3D12GraphicsCommandList2 *target, long int/*instead of D3D12_RESOURCE_STATES to eliminate dependency in d3d12.h here*/ usage = 0) const;
		UINT64 GetGPUPtr() const;	// valid after Reslove if 'usage' was specified accordingly
	};

	template<>
	class QueryBatch<DUAL> final : public QueryBatchBase
	{
		Impl::TrackedResource<ID3D12Resource> batchResults;
		const LPCWSTR name;
		unsigned long version = 0;	// increments within lifetime of batch object, resets to 0 on new object construction
		const long int usages[2];
		const unsigned alignment;

	public:
		// holds reference to the string, designed to pass literal
		QueryBatch(LPCWSTR name, long int firstUsage, long int secondUsage, unsigned alignment = 1u);
		~QueryBatch();

	private:
		void FinalSetup() override;

	public:
		void Set(ID3D12GraphicsCommandList2 *target, unsigned long queryIdx, bool second, bool visible = true) const { QueryBatchBase::Set(target, queryIdx, batchResults.Get(), visible, Offset(second)); }
		void Resolve(ID3D12GraphicsCommandList2 *target, bool second) const;
		UINT64 GetGPUPtr(bool second) const;	// valid after Reslove if corresponding 'usage' was specified accordingly

	private:
		unsigned long Offset(bool second) const noexcept;
	};
}