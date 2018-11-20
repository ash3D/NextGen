#pragma once

#include "stdafx.h"
#include "frame versioning.h"

// not thread-safe
namespace Renderer::CmdListPool
{
	class CmdList
	{
		Impl::CmdCtx *cmdCtx;
		void (CmdList::*setup)(ID3D12PipelineState *PSO) = &CmdList::Init, (CmdList::*setupSimple)(ID3D12PipelineState *) = &CmdList::Init;
		size_t poolIdx;	// for name generation

	public:
		explicit CmdList();
		inline CmdList(CmdList &&src);
		inline CmdList &operator =(CmdList &&src);

	public:
		inline operator ID3D12GraphicsCommandList2 *() const;
		inline const auto &operator ->() const;

	public:
		inline void Setup(ID3D12PipelineState *PSO), Setup();

	public:
		void ResourceBarrier(const D3D12_RESOURCE_BARRIER &barrier), ResourceBarrier(std::initializer_list<D3D12_RESOURCE_BARRIER> barriers);
		template<bool force = false>	// specify true if it is guaranteed there is pending barriers
		void FlushBarriers();

	private:
		void Init(ID3D12PipelineState *PSO), Update(ID3D12PipelineState *PSO);
		void NOP(ID3D12PipelineState *) noexcept {}
	};

	void OnFrameFinish();
}