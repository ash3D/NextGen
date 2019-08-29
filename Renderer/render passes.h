#pragma once

#include "stdafx.h"

namespace Renderer::Impl::CmdListPool
{
	class CmdList;
}

namespace Renderer::Impl::RenderPipeline::RenderPasses
{
	class PipelineROPTargets
	{
		friend class StageRTBinding;
		friend class StageZBinding;
		friend class StageOutput;

	private:
		ID3D12Resource *renderTarget, *ZBuffer, *MSAAResolveTarget;	// !: no lifetime tracking
		D3D12_CPU_DESCRIPTOR_HANDLE rtv, dsv;
		FLOAT colorClear[4], depthClear;
		UINT8 stencilClear;
		UINT width, height;

	private:
		mutable D3D12_RENDER_PASS_ENDING_ACCESS_TYPE *lastRTPostOp{}, *lastDeptPostOp{}, *lastStencilPostOp{};

	public:
		explicit PipelineROPTargets(ID3D12Resource *renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const FLOAT (&colorClear)[4],
			ID3D12Resource *ZBuffer, D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depthClear, UINT8 stencilClear,
			ID3D12Resource *MSAAResolveTarget, UINT width, UINT height);
		PipelineROPTargets(PipelineROPTargets &&) = default;
		PipelineROPTargets &operator =(PipelineROPTargets &&) = default;

	public:
		ID3D12Resource *GetZBuffer() const noexcept { return ZBuffer; }
	};

	class StageRTBinding
	{
		ID3D12Resource *const renderTarget, *const MSAAResolveTarget;
		const D3D12_CPU_DESCRIPTOR_HANDLE rtv;
		FLOAT clear[4];
		D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS resolveParams;
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE preOp;
		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE postOp;

	public:
		explicit StageRTBinding(const PipelineROPTargets &factory);
		StageRTBinding(StageRTBinding &) = delete;
		void operator =(StageRTBinding &) = delete;

	public:
		D3D12_RENDER_PASS_RENDER_TARGET_DESC RenderPassBinding(bool open, bool close) const;
		void FastForward(CmdListPool::CmdList &target, bool open, bool close) const;
		void Finish(CmdListPool::CmdList &target) const;

	private:
		void MSAAResolve(CmdListPool::CmdList &target) const;
	};

	class StageZBinding
	{
		ID3D12Resource *const ZBuffer;
		const D3D12_CPU_DESCRIPTOR_HANDLE dsv;
		D3D12_DEPTH_STENCIL_VALUE clear;
		struct
		{
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE pre;
			D3D12_RENDER_PASS_ENDING_ACCESS_TYPE post;
		} depthOps, stencilOps;

	public:
		explicit StageZBinding(const PipelineROPTargets &factory, bool useDepth, bool useStencil);
		explicit StageZBinding(const PipelineROPTargets &factory, D3D12_CLEAR_FLAGS clearFlags, D3D12_DEPTH_STENCIL_VALUE clear, bool preserveDepth, bool preserveStencil);
		StageZBinding(StageZBinding &) = delete;
		void operator =(StageZBinding &) = delete;

	public:
		ID3D12Resource *GetZBuffer() const noexcept { return ZBuffer; }

	public:
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC RenderPassBinding(bool open, bool close) const;
		void FastForward(CmdListPool::CmdList &target, bool open, bool close) const;
	};

	class StageOutput
	{
		unsigned width, height;

	public:
		explicit StageOutput(const PipelineROPTargets &factory) : width(factory.width), height(factory.height) {}

	public:
		void Setup(CmdListPool::CmdList &target) const;
	};

	template<class StageBinding, template<class> typename Modifier = std::add_lvalue_reference_t>
	struct PassROPBinding
	{
		Modifier<const StageBinding> stageBinding;
		bool open, close;
	};

	class RenderPass
	{
		PassROPBinding<StageRTBinding, std::add_pointer_t> RTBinding;
		PassROPBinding<StageZBinding, std::add_lvalue_reference_t> ZBinding;
		const StageOutput &output;
		bool rangeOpen, rangeClose;

	public:
		explicit RenderPass(const PassROPBinding<StageRTBinding> *RTBinding, const PassROPBinding<StageZBinding> &ZBinding, const StageOutput &output, bool rangeOpen, bool rangeClose);

	public:
		bool Suspended() const noexcept;
		void operator ()(CmdListPool::CmdList &target) const;
		void Finish(CmdListPool::CmdList &target) const;

	private:
		inline D3D12_RENDER_PASS_FLAGS Flags() const noexcept;
	};

	class RenderPassScope
	{
		CmdListPool::CmdList &cmdList;
		const RenderPass &renderPass;	// for MSAA resolve workaround only

	public:
		explicit RenderPassScope(CmdListPool::CmdList &target, const RenderPass &renderPass);
		RenderPassScope(RenderPassScope &) = delete;
		void operator =(RenderPassScope &) = delete;
		~RenderPassScope();
	};
}