#include "stdafx.h"
#include "render passes.h"
#include "cmdlist pool.inl"

static constexpr bool forceCombineROPWrites = false;

using namespace std;
using namespace Renderer::Impl;
using namespace RenderPipeline;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;

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

RenderPasses::PipelineROPTargets::PipelineROPTargets(ID3D12Resource *renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const FLOAT (&colorClear)[4],
	ID3D12Resource *ZBuffer, D3D12_CPU_DESCRIPTOR_HANDLE dsv, ID3D12Resource *MSAAResolveTarget, UINT width, UINT height) :
	renderTarget(renderTarget), ZBuffer(ZBuffer), MSAAResolveTarget(MSAAResolveTarget), rtv(rtv), dsv(dsv),
	colorClear{ colorClear[0], colorClear[1], colorClear[2], colorClear[3] },
	width(width), height(height)
{
}

RenderPasses::StageRTBinding::StageRTBinding(const PipelineROPTargets &factory) :
	renderTarget(factory.renderTarget), MSAAResolveTarget(factory.MSAAResolveTarget), rtv(factory.rtv), clear{ factory.colorClear[0], factory.colorClear[1], factory.colorClear[2], factory.colorClear[3] },
	resolveParams{ .SrcRect = CD3DX12_RECT(0, 0, factory.width, factory.height) },
	preOp(factory.lastRTPostOp ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR),
	postOp(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
{
	if (factory.lastRTPostOp)
		*factory.lastRTPostOp = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	factory.lastRTPostOp = &postOp;
}

D3D12_RENDER_PASS_RENDER_TARGET_DESC RenderPasses::StageRTBinding::RenderPassBinding(bool open, bool close) const
{
	const auto format = renderTarget->GetDesc().Format;
	return
	{
		.cpuDescriptor = rtv,
		.BeginningAccess =
		{
			.Type = FixOp(preOp, open),
			.Clear{format, {.Color{clear[0], clear[1], clear[2], clear[3]}}}
		},
		.EndingAccess =
		{
			.Type = DecayMSSAAResolveOp(FixOp(postOp, close)),
			.Resolve{renderTarget, MSAAResolveTarget, 1, &resolveParams, format, D3D12_RESOLVE_MODE_AVERAGE, FALSE}
		}
	};
}

void RenderPasses::StageRTBinding::FastForward(CmdListPool::CmdList &cmdList, bool open, bool close) const
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
		case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR:
			cmdList->ClearRenderTargetView(rtv, clear, 0, NULL);
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

void RenderPasses::StageRTBinding::Finish(CmdListPool::CmdList &cmdList) const
{
	if (postOp == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE && MSAAResolveWorkaroundNeeded())
	{
		cmdList->ClearState(NULL);
		MSAAResolve(cmdList);
	}
}

void RenderPasses::StageRTBinding::MSAAResolve(CmdListPool::CmdList &cmdList) const
{
	cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE));
	cmdList.FlushBarriers<true>();
	cmdList->ResolveSubresource(MSAAResolveTarget, 0, renderTarget, 0, renderTarget->GetDesc().Format);
	cmdList.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));	// !: use split barrier
}

// propagate if used
RenderPasses::StageZBinding::StageZBinding(const PipelineROPTargets &factory, bool useDepth, bool useStencil) :
	ZBuffer(factory.ZBuffer), dsv(factory.dsv), clear{},
	depthOps
	{
		useDepth	? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE	: D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
		useDepth	? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE		: D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS
	},
	stencilOps
	{
		useStencil	? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE	: D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
		useStencil	? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD		: D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS
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
RenderPasses::StageZBinding::StageZBinding(const PipelineROPTargets &factory, D3D12_CLEAR_FLAGS clearFlags, D3D12_DEPTH_STENCIL_VALUE clear, BindingOutput depthOutput, BindingOutput stencilOutput) :
	ZBuffer(factory.ZBuffer), dsv(factory.dsv), clear(clear),
	depthOps
	{
		clearFlags & D3D12_CLEAR_FLAG_DEPTH				? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR	: D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE,
		depthOutput		== BindingOutput::ForceDiscard	? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD	: D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
	},
	stencilOps
	{
		clearFlags & D3D12_CLEAR_FLAG_STENCIL			? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR	: D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE,
		stencilOutput	== BindingOutput::ForcePreserve	? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE	: D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD
	}
{
	if (factory.lastDepthPostOp)
		*factory.lastDepthPostOp	= clearFlags & D3D12_CLEAR_FLAG_DEPTH		? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	factory.lastDepthPostOp			= depthOutput	== BindingOutput::Propagate	? &depthOps.post	: nullptr;

	if (factory.lastStencilPostOp)
		*factory.lastStencilPostOp	= clearFlags & D3D12_CLEAR_FLAG_STENCIL		? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
	factory.lastStencilPostOp		= stencilOutput	== BindingOutput::Propagate	? &stencilOps.post	: nullptr;
}

D3D12_RENDER_PASS_DEPTH_STENCIL_DESC RenderPasses::StageZBinding::RenderPassBinding(bool open, bool close) const
{
	assert(depthOps.post != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE);
	assert(stencilOps.post != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE);
	const auto format = ZBuffer->GetDesc().Format;
	return
	{
		.cpuDescriptor = dsv,
		.DepthBeginningAccess =
		{
			.Type = FixOp(depthOps.pre, open),
			.Clear{format, {.DepthStencil = clear}}
		},
		.StencilBeginningAccess =
		{
			.Type = FixOp(stencilOps.pre, open),
			.Clear{format, {.DepthStencil = clear}}
		},
		.DepthEndingAccess{FixOp(depthOps.post, close)},
		.StencilEndingAccess{FixOp(stencilOps.post, close)}
	};
}

void RenderPasses::StageZBinding::FastForward(CmdListPool::CmdList &cmdList, bool open, bool close) const
{
	assert(depthOps.post != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE);
	assert(stencilOps.post != D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE);

	D3D12_DISCARD_REGION discardRegion{.FirstSubresource = UINT_MAX};
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

void RenderPasses::StageOutput::Setup(CmdListPool::CmdList &cmdList) const
{
	const CD3DX12_VIEWPORT viewport(0.f, 0.f, width, height);
	const CD3DX12_RECT scissorRect(0, 0, width, height);
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
}

RenderPasses::RenderPass::RenderPass(const PassROPBinding<StageRTBinding> *RTBinding, const PassROPBinding<StageZBinding> &ZBinding, const StageOutput &output, bool rangeOpen, bool rangeClose) :
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

bool RenderPasses::RenderPass::Suspended() const noexcept
{
	return Flags() & D3D12_RENDER_PASS_FLAG_SUSPENDING_PASS;
}

void RenderPasses::RenderPass::operator()(CmdListPool::CmdList &cmdList) const
{
	cmdList->BeginRenderPass(bool(RTBinding.stageBinding),
		RTBinding.stageBinding ? &RTBinding.stageBinding->RenderPassBinding(RTBinding.open, RTBinding.close) : NULL,
		&ZBinding.stageBinding.RenderPassBinding(ZBinding.open, ZBinding.close), Flags());
	output.Setup(cmdList);
}

void RenderPasses::RenderPass::Finish(CmdListPool::CmdList &cmdList) const
{
	if (rangeClose && RTBinding.stageBinding && RTBinding.close)
		RTBinding.stageBinding->Finish(cmdList);
}

inline D3D12_RENDER_PASS_FLAGS RenderPasses::RenderPass::Flags() const noexcept
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

RenderPasses::RenderPassScope::RenderPassScope(CmdListPool::CmdList &cmdList, const RenderPass &renderPass) : cmdList(cmdList), renderPass(renderPass)
{
	renderPass(cmdList);
}

RenderPasses::RenderPassScope::~RenderPassScope()
{
	cmdList->EndRenderPass();
	renderPass.Finish(cmdList);
}