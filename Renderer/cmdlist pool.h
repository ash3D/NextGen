#pragma once

#include <vector>
#include <deque>
#include <initializer_list>
#include "../cmd buffer.h"

struct ID3D12PipelineState;
struct ID3D12GraphicsCommandList4;
struct D3D12_RESOURCE_BARRIER;

// not thread-safe
namespace Renderer::Impl::CmdListPool
{
	struct CmdCtx : private CmdBuffer<>
	{
		friend class CmdList;

	private:
		std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers;
		unsigned long long listVersion = 0;	// for name generation
		bool suspended = false;
	};

	class PerFramePool
	{
		friend class CmdList;
		friend void OnFrameFinish();

	private:
		std::deque<struct CmdCtx> ctxPool;
		unsigned short ringIdx = 0;

	private:
		static decltype(ctxPool)::size_type firstFreePoolIdx;

	public:
		PerFramePool() = default;
		PerFramePool(PerFramePool &&src);
		PerFramePool &operator =(PerFramePool &&src);
	};

	class CmdList
	{
		struct CmdCtx *cmdCtx;
		void (CmdList::*setup)(ID3D12PipelineState *PSO) = &CmdList::Init, (CmdList::*setupSimple)(ID3D12PipelineState *) = &CmdList::Init;
		size_t poolIdx;			// for name generation
		unsigned short ringIdx;	// for name generation

	public:
		explicit CmdList();
		inline CmdList(CmdList &&src);
		inline CmdList &operator =(CmdList &&src);

	public:
		inline operator ID3D12GraphicsCommandList4 *() const;
		inline const auto &operator ->() const;

	public:
		inline void Setup(ID3D12PipelineState *PSO), Setup();

	public:
		void ResourceBarrier(const D3D12_RESOURCE_BARRIER &barrier), ResourceBarrier(std::initializer_list<D3D12_RESOURCE_BARRIER> barriers);
		template<bool force = false>	// specify true if it is guaranteed there is pending barriers
		void FlushBarriers();

	public:
		inline void MarkSuspended(bool suspended);

	private:
		void Init(ID3D12PipelineState *PSO), Update(ID3D12PipelineState *PSO);
		void NOP(ID3D12PipelineState *) noexcept {}
	};

	void OnFrameFinish();
}