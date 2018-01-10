/**
\author		Alexey Shaydurov aka ASH
\date		10.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "viewport.hh"
#include "world.hh"
#include "GPU work submission.h"
#include "render pipeline.h"

using namespace std;
using namespace Renderer;
using WRL::ComPtr;
namespace RenderPipeline = Impl::RenderPipeline;

extern ComPtr<ID3D12Device2> device;

static inline RenderPipeline::PipelineStage Pre(ID3D12GraphicsCommandList1 *cmdList, ID3D12Resource *rt, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(rt, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	const float color[4] = { .0f, .2f, .4f, 1.f };
	cmdList->ClearRenderTargetView(rtv, color, 0, NULL);
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_STENCIL, 1.f, UINT8_MAX, 0, NULL);
	CheckHR(cmdList->Close());
	return cmdList;
}

static inline RenderPipeline::PipelineStage Post(ID3D12GraphicsCommandList1 *cmdList, ID3D12Resource *rt)
{
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(rt, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	CheckHR(cmdList->Close());
	return cmdList;
}

// result valid until call to 'OnFrameFinish()'
auto Impl::Viewport::CmdListsManager::OnFrameStart() -> PrePostCmds<ID3D12GraphicsCommandList1 *>
{
	FrameVersioning::OnFrameStart();
	auto &cmdBuffers = GetCurFrameDataVersion();

	if (cmdBuffers.pre.allocator && cmdBuffers.post.allocator && cmdBuffers.pre.list && cmdBuffers.post.list)
	{
		// reset allocators
		CheckHR(cmdBuffers.pre.allocator->Reset());
		CheckHR(cmdBuffers.post.allocator->Reset());

		// reset lists
		CheckHR(cmdBuffers.pre.list->Reset(cmdBuffers.pre.allocator.Get(), NULL));
		CheckHR(cmdBuffers.post.list->Reset(cmdBuffers.post.allocator.Get(), NULL));
	}
	else
	{
		// create allocators
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdBuffers.pre.allocator)));
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdBuffers.post.allocator)));

		// create lists
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdBuffers.pre.allocator.Get(), NULL, IID_PPV_ARGS(&cmdBuffers.pre.list)));
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdBuffers.post.allocator.Get(), NULL, IID_PPV_ARGS(&cmdBuffers.post.list)));
	}

	return { cmdBuffers.pre.list.Get(), cmdBuffers.post.list.Get() };
}

Impl::Viewport::Viewport(shared_ptr<const Renderer::World> world) : world(move(world)), viewXform(), projXform()
{
	viewXform[0][0] = viewXform[1][1] = viewXform[2][2] = projXform[0][0] = projXform[1][1] = projXform[2][2] = projXform[3][3] = 1.f;
}

void Impl::Viewport::SetViewTransform(const float (&matrix)[4][3])
{
	memcpy(viewXform, matrix, sizeof viewXform);
}

void Impl::Viewport::SetProjectionTransform(double fovy, double zn, double zf)
{
	assert(isgreater(fovy, 0.));
	assert(isgreater(zn, 0.));
	assert(isgreater(zf, 0.));
	assert(isless(zn, zf));

	const double yScale = 1. / tan(fovy / 2.), invAspect = double(projXform[0][0]) / double(projXform[1][1]), zScale = isfinite(zf) ? zf / (zf - zn) : 1.;
	projXform[0][0] = yScale * invAspect;
	projXform[1][1] = yScale;
	projXform[2][2] = zScale;
	projXform[3][2] = -zn * zScale;
	projXform[2][3] = 1;
	projXform[3][3] = 0;
}

void Impl::Viewport::UpdateAspect(double invAspect)
{
	projXform[0][0] = projXform[1][1] * invAspect;
}

void Impl::Viewport::Render(ID3D12Resource *rt, const D3D12_CPU_DESCRIPTOR_HANDLE rtv, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, UINT width, UINT height) const
{
	auto cmdLists = cmdListsManager.OnFrameStart();
	GPUWorkSubmission::Prepare();

	GPUWorkSubmission::AppendCmdList(Pre, cmdLists.pre, rt, rtv, dsv);

	const function<void (bool enableRT, ID3D12GraphicsCommandList1 *target)> setupRenderOutputCallback =
		[
			rtv, dsv,
			viewport = CD3DX12_VIEWPORT(0.f, 0.f, width, height),
			scissorRect = CD3DX12_RECT(0, 0, width, height)
		](bool enableRT, ID3D12GraphicsCommandList1 *cmdList)
	{
		cmdList->OMSetRenderTargets(enableRT, &rtv, TRUE, &dsv);
		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);
	};
	if (world)
		world->Render(viewXform, projXform, setupRenderOutputCallback);

	GPUWorkSubmission::AppendCmdList(Post, cmdLists.post, rt);

	GPUWorkSubmission::Run();
	cmdListsManager.OnFrameFinish();
}

void Impl::Viewport::OnFrameFinish() const
{
	if (world)
		world->OnFrameFinish();
}