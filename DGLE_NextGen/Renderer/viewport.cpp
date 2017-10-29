/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

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

static inline RenderPipeline::PipelineStage Pre(ID3D12GraphicsCommandList1 *cmdList, ID3D12Resource *rt, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const function<void (ID3D12GraphicsCommandList1 *target)> &terrainBaseRenderCallback)
{
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(rt, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	const float color[4] = { .0f, .2f, .4f, 1.f };
	cmdList->ClearRenderTargetView(rtv, color, 0, NULL);
	if (terrainBaseRenderCallback)
		terrainBaseRenderCallback(cmdList);
	CheckHR(cmdList->Close());
	return cmdList;
}

static inline RenderPipeline::PipelineStage Post(ID3D12GraphicsCommandList1 *cmdList, ID3D12Resource *rt)
{
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(rt, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	CheckHR(cmdList->Close());
	return cmdList;
}

Impl::Viewport::CmdListsManager::CmdListsManager()
{
	auto &allocs = GetCmdAllocators();
	CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocs.pre.Get(), NULL, IID_PPV_ARGS(&cmdLists.pre)));
	CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocs.post.Get(), NULL, IID_PPV_ARGS(&cmdLists.post)));
}

// result valid until call to 'OnFrameFinish()'
auto Impl::Viewport::CmdListsManager::OnFrameStart() -> PrePostCmds<ID3D12GraphicsCommandList1 *>
{
	assert(cmdLists.pre && cmdLists.post);
	FrameVersioning::OnFrameStart();

	// skip first time (list created ready to use in ctor)
	if (GetCurFrameID() > 1)
	{
		auto &allocs = GetCmdAllocators();
		CheckHR(cmdLists.pre->Reset(allocs.pre.Get(), NULL));
		CheckHR(cmdLists.post->Reset(allocs.post.Get(), NULL));
	}

	return { cmdLists.pre.Get(), cmdLists.post.Get() };
}

auto Impl::Viewport::CmdListsManager::GetCmdAllocators() -> decltype(GetCurFrameDataVersion())
{
	auto &allocs = GetCurFrameDataVersion();
	if (allocs.pre && allocs.post)
	{
		CheckHR(allocs.pre->Reset());
		CheckHR(allocs.post->Reset());
	}
	else
	{
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocs.pre)));
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocs.post)));
	}
	return allocs;
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

void Impl::Viewport::Render(ID3D12Resource *rt, const D3D12_CPU_DESCRIPTOR_HANDLE &rtv, UINT width, UINT height) const
{
	auto cmdLits = cmdListsManager.OnFrameStart();
	GPUWorkSubmission::Prepare();

	function<void (ID3D12GraphicsCommandList1 *target)> terrainBaseRenderCallback;
	GPUWorkSubmission::AppendCmdList(Pre, cmdLits.pre, rt, rtv, cref(terrainBaseRenderCallback));

	const function<void (ID3D12GraphicsCommandList1 *target)> setupRenderOutputCallback =
		[
			rtv,
			viewport = CD3DX12_VIEWPORT(0.f, 0.f, width, height),
			scissorRect = CD3DX12_RECT(0, 0, width, height)
		](ID3D12GraphicsCommandList1 *cmdList)
	{
		cmdList->OMSetRenderTargets(1, &rtv, TRUE, NULL);
		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);
	};
	if (world)
		terrainBaseRenderCallback = world->Render(viewXform, projXform, setupRenderOutputCallback);

	GPUWorkSubmission::AppendCmdList(Post, cmdLits.post, rt);

	GPUWorkSubmission::Run();
	cmdListsManager.OnFrameFinish();
}