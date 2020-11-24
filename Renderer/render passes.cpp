#include "stdafx.h"
#include "render passes.h"
#include "cmdlist pool.inl"

static constexpr bool forceCombineROPWrites = false;

using namespace std;
using namespace Renderer::Impl;
using namespace RenderPipeline;
using WRL::ComPtr;

extern ComPtr<ID3D12Device4> device;

static bool MSAAResolveWorkaroundNeeded()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS2 caps;
	CheckHR(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &caps, sizeof caps));
	return caps.ProgrammableSamplePositionsTier == D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED;
}

static bool RenderPassDriverSupport()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 support;
	CheckHR(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &support, sizeof support));
	return support.RenderPassesTier != D3D12_RENDER_PASS_TIER_0;
}

static bool TBDR_GPU()
{
	D3D12_FEATURE_DATA_ARCHITECTURE GPUArch{};
	CheckHR(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &GPUArch, sizeof GPUArch));
	return GPUArch.TileBasedRenderer;
}

static bool CombineROPWrites()
{
	return forceCombineROPWrites || RenderPassDriverSupport() && TBDR_GPU();
}

static inline D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE FixOp(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE preOp, bool open) noexcept
{
	return open || preOp == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS ? preOp : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
}

static inline D3D12_RENDER_PASS_ENDING_ACCESS_TYPE FixOp(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE postOp, bool close) noexcept
{
	return close || postOp == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS ? postOp : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
}

static inline D3D12_RENDER_PASS_ENDING_ACCESS_TYPE DecayMSSAAResolveOp(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE postOp)
{
	return postOp == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE && MSAAResolveWorkaroundNeeded() ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : postOp;
}

RenderPasses::PipelineROPTargets::PipelineROPTargets(ID3D12Resource *renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtv, ID3D12Resource *ZBuffer, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
	ID3D12Resource *MSAAResolveTarget, UINT width, UINT height) :
	renderTarget(renderTarget), ZBuffer(ZBuffer), MSAAResolveTarget(MSAAResolveTarget), rtv(rtv), dsv(dsv),
	width(width), height(height)
{
}

#pragma region PipelineStageRTBinding
RenderPasses::PipelineStageRTBinding::PipelineStageRTBinding(const PipelineROPTargets &factory) :
	renderTarget(factory.renderTarget), MSAAResolveTarget(factory.MSAAResolveTarget), rtv(factory.rtv),
	resolveParams{ .SrcRect = CD3DX12_RECT(0, 0, factory.width, factory.height) },
	preOp(factory.lastRTPostOp ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD),
	postOp(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
{
	if (factory.lastRTPostOp)
		*factory.lastRTPostOp = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	factory.lastRTPostOp = &postOp;
}

D3D12_RENDER_PASS_RENDER_TARGET_DESC RenderPasses::PipelineStageRTBinding::RenderPassBinding() const
{
	const auto format = renderTarget->GetDesc().Format;
	return
	{
		.cpuDescriptor = rtv,
		.BeginningAccess{preOp},
		.EndingAccess =
		{
			.Type = DecayMSSAAResolveOp(postOp),
			.Resolve{renderTarget, MSAAResolveTarget, 1, &resolveParams, format, D3D12_RESOLVE_MODE_AVERAGE, FALSE}
		}
	};
}

template<class CmdList>
void RenderPasses::PipelineStageRTBinding::Finish(CmdList &cmdList) const
{
	if (postOp == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE && MSAAResolveWorkaroundNeeded())
	{
		cmdList->ClearState(NULL);
		MSAAResolve(cmdList);
	}
}

template<class CmdList>
void RenderPasses::PipelineStageRTBinding::MSAAResolve(CmdList &cmdList) const
{
	cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE));
	cmdList.FlushBarriers<true>();
	cmdList->ResolveSubresource(MSAAResolveTarget, 0, renderTarget, 0, renderTarget->GetDesc().Format);
	cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));	// !: use split barrier
}
#pragma endregion

#pragma region PipelineStageZBinding
// propagate if used
RenderPasses::PipelineStageZBinding::PipelineStageZBinding(const PipelineROPTargets &factory, bool useDepth, bool useStencil) :
	ZBuffer(factory.ZBuffer), dsv(factory.dsv), clear{},
	depthOps
{
	useDepth ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
	useDepth ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS
},
stencilOps
{
	useStencil ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
	useStencil ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS
}
{
	if (useDepth)
	{
		if (factory.lastDepthPostOp)
			*factory.lastDepthPostOp = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		factory.lastDepthPostOp = &depthOps.post;
	}

	if (useStencil)
	{
		if (factory.lastStencilPostOp)
			*factory.lastStencilPostOp = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		factory.lastStencilPostOp = &stencilOps.post;
	}
}

// preserve if not clear, use both depth & stencil
RenderPasses::PipelineStageZBinding::PipelineStageZBinding(const PipelineROPTargets &factory, D3D12_CLEAR_FLAGS clearFlags, D3D12_DEPTH_STENCIL_VALUE clear, BindingOutput depthOutput, BindingOutput stencilOutput) :
	ZBuffer(factory.ZBuffer), dsv(factory.dsv), clear(clear),
	depthOps
{
	clearFlags & D3D12_CLEAR_FLAG_DEPTH ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE,
	depthOutput == BindingOutput::ForceDiscard ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
},
stencilOps
{
	clearFlags & D3D12_CLEAR_FLAG_STENCIL ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE,
	stencilOutput == BindingOutput::ForcePreserve ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD
}
{
	if (factory.lastDepthPostOp)
		*factory.lastDepthPostOp = clearFlags & D3D12_CLEAR_FLAG_DEPTH ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	factory.lastDepthPostOp = depthOutput == BindingOutput::Propagate ? &depthOps.post : nullptr;

	if (factory.lastStencilPostOp)
		*factory.lastStencilPostOp = clearFlags & D3D12_CLEAR_FLAG_STENCIL ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	factory.lastStencilPostOp = stencilOutput == BindingOutput::Propagate ? &stencilOps.post : nullptr;
}

D3D12_RENDER_PASS_DEPTH_STENCIL_DESC RenderPasses::PipelineStageZBinding::RenderPassBinding() const
{
	assert(depthOps.post != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE);
	assert(stencilOps.post != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE);
	const auto format = ZBuffer->GetDesc().Format;
	return
	{
		.cpuDescriptor = dsv,
		.DepthBeginningAccess =
		{
			.Type = depthOps.pre,
			.Clear{format, {.DepthStencil = clear}}
		},
		.StencilBeginningAccess =
		{
			.Type = stencilOps.pre,
			.Clear{format, {.DepthStencil = clear}}
		},
		.DepthEndingAccess{depthOps.post},
		.StencilEndingAccess{stencilOps.post}
	};
}
#pragma endregion

#pragma region RenderStageRTBinding
D3D12_RENDER_PASS_RENDER_TARGET_DESC RenderPasses::RenderStageRTBinding::RenderPassBinding(bool open, bool close) const
{
	D3D12_RENDER_PASS_RENDER_TARGET_DESC RTBinding = RenderPassBinding();
	RTBinding.BeginningAccess.Type = FixOp(RTBinding.BeginningAccess.Type, open);
	RTBinding.EndingAccess.Type = FixOp(RTBinding.EndingAccess.Type, close);
	return RTBinding;
}

void RenderPasses::RenderStageRTBinding::FastForward(CmdListPool::CmdList &cmdList, bool open, bool close) const
{
	const auto ApplyPreOp = [&, open]
	{
		switch (FixOp(preOp, open))
		{
		case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD:
		{
			const D3D12_DISCARD_REGION region = { 0, NULL, 0, 1 };
			cmdList->DiscardResource(renderTarget, &region);
		}
		break;
		}
	};

	switch (FixOp(postOp, close))
	{
	case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD:
	{
		const D3D12_DISCARD_REGION region = { 0, NULL, 0, 1 };
		cmdList->DiscardResource(renderTarget, &region);
	}
	break;
	case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE:
		ApplyPreOp();
		break;
	case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE:
		ApplyPreOp();
		MSAAResolve(cmdList);
		break;
	}
}
#pragma endregion

#pragma region RenderStageZBinding
D3D12_RENDER_PASS_DEPTH_STENCIL_DESC RenderPasses::RenderStageZBinding::RenderPassBinding(bool open, bool close) const
{
	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC ZBinding = RenderPassBinding();
	ZBinding.DepthBeginningAccess.Type = FixOp(ZBinding.DepthBeginningAccess.Type, open);
	ZBinding.StencilBeginningAccess.Type = FixOp(ZBinding.StencilBeginningAccess.Type, open);
	ZBinding.DepthEndingAccess.Type = FixOp(ZBinding.DepthEndingAccess.Type, close);
	ZBinding.StencilEndingAccess.Type = FixOp(ZBinding.StencilEndingAccess.Type, close);
	return ZBinding;
}

void RenderPasses::RenderStageZBinding::FastForward(CmdListPool::CmdList &cmdList, bool open, bool close) const
{
	assert(depthOps.post != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE);
	assert(stencilOps.post != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE);

	D3D12_DISCARD_REGION discardRegion{ .FirstSubresource = UINT_MAX };
	D3D12_CLEAR_FLAGS clearFlags{};

	const auto ApplyPreOp = [&, open](UINT subresource, D3D12_CLEAR_FLAGS clearFlag, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE preOp)
	{
		switch (FixOp(preOp, open))
		{
		case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD:
			discardRegion.FirstSubresource = min(discardRegion.FirstSubresource, subresource);
			discardRegion.NumSubresources++;
			break;
		case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR:
			clearFlags |= clearFlag;
			break;
		}
	};

	const auto ApplyPrePostOps = [&, close](UINT subresource, D3D12_CLEAR_FLAGS clearFlag, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE preOp, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE postOp)
	{
		switch (FixOp(postOp, close))
		{
		case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD:
			discardRegion.FirstSubresource = min(discardRegion.FirstSubresource, subresource);
			discardRegion.NumSubresources++;
			break;
		case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE:
			ApplyPreOp(subresource, clearFlag, preOp);
			break;
		}
	};

	ApplyPrePostOps(0, D3D12_CLEAR_FLAG_DEPTH, depthOps.pre, depthOps.post);
	ApplyPrePostOps(1, D3D12_CLEAR_FLAG_STENCIL, stencilOps.pre, stencilOps.post);

	if (discardRegion.NumSubresources)
	{
		assert(discardRegion.FirstSubresource + discardRegion.NumSubresources <= 2);
		cmdList->DiscardResource(ZBuffer, &discardRegion);
	}

	if (clearFlags)
		cmdList->ClearDepthStencilView(dsv, clearFlags, clear.Depth, clear.Stencil, 0, NULL);
}
#pragma endregion

void RenderPasses::ROPOutput::Setup(ID3D12GraphicsCommandList4 *cmdList) const
{
	const CD3DX12_VIEWPORT viewport(0.f, 0.f, width, height);
	const CD3DX12_RECT scissorRect(0, 0, width, height);
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
}

#pragma region range render pass
RenderPasses::RangeRenderPass::RangeRenderPass(const PassROPBinding<RenderStageRTBinding> *RTBinding, const PassROPBinding<RenderStageZBinding> &ZBinding, const ROPOutput &output, bool rangeOpen, bool rangeClose) :
	RTBinding(RTBinding ? decltype(this->RTBinding){&RTBinding->stageBinding, RTBinding->open, RTBinding->close} : decltype(this->RTBinding){}),
	ZBinding(ZBinding),
	output(output),
	rangeOpen(rangeOpen), rangeClose(rangeClose)
{
	if (!CombineROPWrites())
	{
		this->RTBinding.open &= rangeOpen;
		this->RTBinding.close &= rangeClose;
		this->ZBinding.open &= rangeOpen;
		this->ZBinding.close &= rangeClose;
	}
}

bool RenderPasses::RangeRenderPass::Suspended() const noexcept
{
	return Flags() & D3D12_RENDER_PASS_FLAG_SUSPENDING_PASS;
}

void RenderPasses::RangeRenderPass::operator()(CmdListPool::CmdList &cmdList) const
{
	D3D12_RENDER_PASS_RENDER_TARGET_DESC passRTBinding;
	if (RTBinding.stageBinding)
		passRTBinding = RTBinding.stageBinding->RenderPassBinding(RTBinding.open, RTBinding.close);
	const auto passZBinding = ZBinding.stageBinding.RenderPassBinding(ZBinding.open, ZBinding.close);
	cmdList->BeginRenderPass(bool(RTBinding.stageBinding), &passRTBinding, &passZBinding, Flags());
	output.Setup(cmdList);
}

void RenderPasses::RangeRenderPass::Finish(CmdListPool::CmdList &cmdList) const
{
	if (rangeClose && RTBinding.stageBinding && RTBinding.close)
		RTBinding.stageBinding->Finish(cmdList);
}

inline D3D12_RENDER_PASS_FLAGS RenderPasses::RangeRenderPass::Flags() const noexcept
{
	D3D12_RENDER_PASS_FLAGS flags = D3D12_RENDER_PASS_FLAG_NONE;
	if (CombineROPWrites())
	{
		if (!rangeOpen)
			flags |= D3D12_RENDER_PASS_FLAG_RESUMING_PASS;
		if (!rangeClose)
			flags |= D3D12_RENDER_PASS_FLAG_SUSPENDING_PASS;
	}
	return flags;
}

RenderPasses::RangeRenderPassScope::RangeRenderPassScope(CmdListPool::CmdList &cmdList, const RangeRenderPass &renderPass) : cmdList(cmdList), renderPass(renderPass)
{
	renderPass(cmdList);
}

RenderPasses::RangeRenderPassScope::~RangeRenderPassScope()
{
	cmdList->EndRenderPass();
	renderPass.Finish(cmdList);
}
#pragma endregion

#pragma region local render pass
RenderPasses::LocalRenderPassScope::LocalRenderPassScope(ID3D12GraphicsCommandList4 *cmdList, const PipelineStageRTBinding &RTBinding, const PipelineStageZBinding &ZBinding, const ROPOutput &output) :
	cmdList(cmdList), RTBinding(RTBinding)
{
	const auto passRTBinding = RTBinding.RenderPassBinding();
	const auto passZBinding = ZBinding.RenderPassBinding();
	cmdList->BeginRenderPass(1, &passRTBinding, &passZBinding, D3D12_RENDER_PASS_FLAG_NONE);
	output.Setup(cmdList);
}

RenderPasses::LocalRenderPassScope::~LocalRenderPassScope()
{
	cmdList->EndRenderPass();
	RTBinding.Finish(cmdList);
}
#pragma endregion