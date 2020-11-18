#pragma once

#include "stdafx.h"

namespace Renderer::Impl::CmdListPool
{
	class CmdList;
}

namespace Renderer::Impl::RenderPipeline::RenderPasses
{
	enum class BindingOutput
	{
		ForcePreserve,	// e.g. copy from
		ForceDiscard,	// e.g. copy to
		Propagate,		// preserve depth for postprocess, discard stencil
	};

	class PipelineROPTargets final
	{
		friend class PipelineStageRTBinding;
		friend class PipelineStageZBinding;
		friend class ROPOutput;

	private:
		ID3D12Resource *renderTarget, *ZBuffer, *MSAAResolveTarget;	// !: no lifetime tracking
		D3D12_CPU_DESCRIPTOR_HANDLE rtv, dsv;
		UINT width, height;

	private:
		mutable D3D12_RENDER_PASS_ENDING_ACCESS_TYPE *lastRTPostOp{}, *lastDepthPostOp{}, *lastStencilPostOp{};

	public:
		explicit PipelineROPTargets(ID3D12Resource *renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtv, ID3D12Resource *ZBuffer, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
			ID3D12Resource *MSAAResolveTarget, UINT width, UINT height);
		PipelineROPTargets(PipelineROPTargets &&) = default;
		PipelineROPTargets &operator =(PipelineROPTargets &&) = default;

	public:
		ID3D12Resource *GetZBuffer() const noexcept { return ZBuffer; }
	};

	class PipelineStageRTBinding
	{
	protected:
		ID3D12Resource *const renderTarget, *const MSAAResolveTarget;
		const D3D12_CPU_DESCRIPTOR_HANDLE rtv;
		D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS resolveParams;
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE preOp;
		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE postOp;

	public:
		explicit PipelineStageRTBinding(const PipelineROPTargets &factory);
		PipelineStageRTBinding(PipelineStageRTBinding &) = delete;
		void operator =(PipelineStageRTBinding &) = delete;

	public:
		D3D12_RENDER_PASS_RENDER_TARGET_DESC RenderPassBinding() const;
		template<class CmdList>
		void Finish(CmdList &target) const;

	protected:
		template<class CmdList>
		void MSAAResolve(CmdList &target) const;
	};

	class PipelineStageZBinding
	{
	protected:
		ID3D12Resource *const ZBuffer;
		const D3D12_CPU_DESCRIPTOR_HANDLE dsv;
		D3D12_DEPTH_STENCIL_VALUE clear;
		struct
		{
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE pre;
			D3D12_RENDER_PASS_ENDING_ACCESS_TYPE post;
		} depthOps, stencilOps;

	public:
		explicit PipelineStageZBinding(const PipelineROPTargets &factory, bool useDepth, bool useStencil);
		explicit PipelineStageZBinding(const PipelineROPTargets &factory, D3D12_CLEAR_FLAGS clearFlags, D3D12_DEPTH_STENCIL_VALUE clear, BindingOutput depthOutput, BindingOutput stencilOutput);
		PipelineStageZBinding(PipelineStageZBinding &) = delete;
		void operator =(PipelineStageZBinding &) = delete;

	public:
		ID3D12Resource *GetZBuffer() const noexcept { return ZBuffer; }

	public:
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC RenderPassBinding() const;
	};

	class RenderStageRTBinding final : public PipelineStageRTBinding
	{
		using PipelineStageRTBinding::PipelineStageRTBinding;
		using PipelineStageRTBinding::RenderPassBinding;

	public:
		D3D12_RENDER_PASS_RENDER_TARGET_DESC RenderPassBinding(bool open, bool close) const;
		void FastForward(CmdListPool::CmdList &target, bool open, bool close) const;
	};

	class RenderStageZBinding final : public PipelineStageZBinding
	{
		using PipelineStageZBinding::PipelineStageZBinding;
		using PipelineStageZBinding::RenderPassBinding;

	public:
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC RenderPassBinding(bool open, bool close) const;
		void FastForward(CmdListPool::CmdList &target, bool open, bool close) const;
	};

	class ROPOutput final
	{
		unsigned width, height;

	public:
		explicit ROPOutput(const PipelineROPTargets &factory) : width(factory.width), height(factory.height) {}

	public:
		void Setup(ID3D12GraphicsCommandList4 *target) const;
	};

	template<class StageBinding, template<class> typename Modifier = std::add_lvalue_reference_t>
	struct PassROPBinding final
	{
		Modifier<const StageBinding> stageBinding;
		bool open, close;
	};

	class RangeRenderPass final
	{
		friend class RangeRenderPassScope;

	private:
		PassROPBinding<RenderStageRTBinding, std::add_pointer_t> RTBinding;
		PassROPBinding<RenderStageZBinding, std::add_lvalue_reference_t> ZBinding;
		const ROPOutput &output;
		bool rangeOpen, rangeClose;

	public:
		explicit RangeRenderPass(const PassROPBinding<RenderStageRTBinding> *RTBinding, const PassROPBinding<RenderStageZBinding> &ZBinding, const ROPOutput &output, bool rangeOpen, bool rangeClose);

	public:
		bool Suspended() const noexcept;

	private:
		void operator ()(CmdListPool::CmdList &target) const;
		void Finish(CmdListPool::CmdList &target) const;

	private:
		inline D3D12_RENDER_PASS_FLAGS Flags() const noexcept;
	};

	class RangeRenderPassScope final
	{
		CmdListPool::CmdList &cmdList;
		const RangeRenderPass &renderPass;	// for MSAA resolve workaround only

	public:
		explicit RangeRenderPassScope(CmdListPool::CmdList &target, const RangeRenderPass &renderPass);
		RangeRenderPassScope(RangeRenderPassScope &) = delete;
		void operator =(RangeRenderPassScope &) = delete;
		~RangeRenderPassScope();
	};

	class LocalRenderPassScope final
	{
		const class CmdList
		{
			ID3D12GraphicsCommandList4 *const list;

		public:
			CmdList(ID3D12GraphicsCommandList4 *list) noexcept : list(list) {}

		public:
			operator ID3D12GraphicsCommandList4 *() const noexcept { return list; }
			ID3D12GraphicsCommandList4 *operator ->() const noexcept { return list; }

		public:
			void ResourceBarrier(const D3D12_RESOURCE_BARRIER &barrier) const { list->ResourceBarrier(1, &barrier); }
			template<bool>
			void FlushBarriers() const noexcept {/*NOP*/ }
		} cmdList;
		const PipelineStageRTBinding &RTBinding;	// for MSAA resolve workaround only

	public:
		explicit LocalRenderPassScope(ID3D12GraphicsCommandList4 *target, const PipelineStageRTBinding &RTBinding, const PipelineStageZBinding &ZBinding, const ROPOutput &output);
		LocalRenderPassScope(LocalRenderPassScope &) = delete;
		void operator =(LocalRenderPassScope &) = delete;
		~LocalRenderPassScope();
	};
}