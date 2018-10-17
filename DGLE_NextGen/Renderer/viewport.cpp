/**
\author		Alexey Shaydurov aka ASH
\date		16.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "viewport.hh"
#include "world.hh"
#include "GPU work submission.h"
#include "GPU descriptor heap.h"
#include "shader bytecode.h"
#include "config.h"
#include "tonemapTextureReduction config.h"
namespace Renderer::TonemappingConfig
{
#	include "tonemapping config.hlsli"
}

namespace Shaders
{
#	include "tonemapTextureReduction.csh"
#	include "tonemapBufferReduction.csh"
#	include "tonemapping.csh"
}

// offsetof is conditionally supported for non-standard layout types since C++17
#define ENABLE_NONSTDLAYOUT_OFFSETOF 1

using namespace std;
using namespace Renderer;
using WRL::ComPtr;
namespace RenderPipeline = Impl::RenderPipeline;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);
	
extern const float backgroundColor[4] = { .0f, .2f, .4f, 1.f };

// result valid until call to 'OnFrameFinish()'
auto Impl::Viewport::CmdListsManager::OnFrameStart() -> PrePostCmds<ID3D12GraphicsCommandList2 *>
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
		CheckHR(cmdBuffers.post.list->Reset(cmdBuffers.post.allocator.Get(), tonemapTextureReductionPSO.Get()));
	}
	else
	{
#if ENABLE_NONSTDLAYOUT_OFFSETOF
		const void *const ptr = reinterpret_cast<const std::byte *>(this) - offsetof(Viewport, cmdListsManager);
#else
		const void *const ptr = this;
#endif
		const unsigned short version = GetFrameLatency() - 1;

		// create allocators
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdBuffers.pre.allocator)));
		NameObjectF(cmdBuffers.pre.allocator.Get(), L"viewport %p pre command allocator [%hu]", ptr, version);
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdBuffers.post.allocator)));
		NameObjectF(cmdBuffers.post.allocator.Get(), L"viewport %p post command allocator [%hu]", ptr, version);

		// create lists
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdBuffers.pre.allocator.Get(), NULL, IID_PPV_ARGS(&cmdBuffers.pre.list)));
		NameObjectF(cmdBuffers.pre.list.Get(), L"viewport %p pre command list [%hu]", ptr, version);
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdBuffers.post.allocator.Get(), tonemapTextureReductionPSO.Get(), IID_PPV_ARGS(&cmdBuffers.post.list)));
		NameObjectF(cmdBuffers.post.list.Get(), L"viewport %p post command list [%hu]", ptr, version);
	}

	return { cmdBuffers.pre.list.Get(), cmdBuffers.post.list.Get() };
}

#pragma region tonemapping root sig & PSOs
ComPtr<ID3D12RootSignature> Impl::Viewport::CreateTonemapRootSig()
{
	// consider encapsulating desc table in TonemapResourceViewsStage
	CD3DX12_ROOT_PARAMETER1 rootParams[ROOT_PARAM_COUNT];
	const D3D12_DESCRIPTOR_RANGE1 descTable[] =
	{
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE/*for hardware out-of-bounds access checking*/)
	};
	rootParams[ROOT_PARAM_DESC_TABLE].InitAsDescriptorTable(size(descTable), descTable);
	rootParams[ROOT_PARAM_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);	// alternatively can place into desc table with desc static flag
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams);
	return CreateRootSignature(sigDesc, L"tonemapping root signature");
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateTonemapTextureReductionPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		tonemapRootSig.Get(),								// root signature
		ShaderBytecode(Shaders::tonemapTextureReduction),	// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"tonemap texture reduction PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateTonemapBufferReductionPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		tonemapRootSig.Get(),								// root signature
		ShaderBytecode(Shaders::tonemapBufferReduction),	// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"tonemap buffer reduction PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateTonemapPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		tonemapRootSig.Get(),								// root signature
		ShaderBytecode(Shaders::tonemapping),				// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"tonemapping PSO");
	return PSO;
}
#pragma endregion

inline RenderPipeline::PipelineStage Impl::Viewport::Pre(ID3D12GraphicsCommandList2 *cmdList, ID3D12Resource *output, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, UINT width, UINT height)
{
	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPre), "viewport pre");
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY));
	cmdList->ClearRenderTargetView(rtv, backgroundColor, 0, NULL);
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0xef, 0, NULL);
	CheckHR(cmdList->Close());
	return cmdList;
}

inline RenderPipeline::PipelineStage Impl::Viewport::Post(ID3D12GraphicsCommandList2 *cmdList, ID3D12Resource *output, ID3D12Resource *rendertarget, ID3D12Resource *HDRSurface, ID3D12Resource *LDRSurface, ID3D12Resource *tonemapReductionBuffer, D3D12_GPU_DESCRIPTOR_HANDLE tonemapDescriptorTable, UINT width, UINT height)
{
	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPost), "viewport post");

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(rendertarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE));

	cmdList->ResolveSubresource(HDRSurface, 0, rendertarget, 0, Config::HDRFormat);
	
	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(rendertarget, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),	// !: use split barrier
			CD3DX12_RESOURCE_BARRIER::Transition(HDRSurface, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// bind tonemapping resources
	cmdList->SetDescriptorHeaps(1, Descriptors::GPUDescriptorHeap::GetHeap().GetAddressOf());
	cmdList->SetComputeRootSignature(tonemapRootSig.Get());
	cmdList->SetComputeRootDescriptorTable(ROOT_PARAM_DESC_TABLE, tonemapDescriptorTable);
	cmdList->SetComputeRootConstantBufferView(ROOT_PARAM_CBV, tonemapReductionBuffer->GetGPUVirtualAddress());

	// initial texture reduction (PSO set during cmd list creation/reset)
	const auto tonemapReductionTexDispatchSize = ReductionTextureConfig::DispatchSize({ width, height });
	cmdList->Dispatch(tonemapReductionTexDispatchSize.x, tonemapReductionTexDispatchSize.y, 1);

	// UAV barrier
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(tonemapReductionBuffer));

	// final buffer reduction
	cmdList->SetPipelineState(tonemapBufferReductionPSO.Get());
	cmdList->Dispatch(1, 1, 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tonemapReductionBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// tonemapping main pass
	cmdList->SetPipelineState(tonemapPSO.Get());
	cmdList->Dispatch((width + TonemappingConfig::blockSize - 1) / TonemappingConfig::blockSize, (height + TonemappingConfig::blockSize - 1) / TonemappingConfig::blockSize, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(HDRSurface, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST),					// !: use split barrier
			CD3DX12_RESOURCE_BARRIER::Transition(LDRSurface, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(tonemapReductionBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),	// !: use split barrier
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// copy to output
	cmdList->CopyTextureRegion(&CD3DX12_TEXTURE_COPY_LOCATION(output, 0), 0, 0, 0, &CD3DX12_TEXTURE_COPY_LOCATION(LDRSurface, 0), NULL);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(LDRSurface, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),	// !: use split barrier
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	CheckHR(cmdList->Close());
	return cmdList;
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

void Impl::Viewport::Render(ID3D12Resource *output, ID3D12Resource *rendertarget, ID3D12Resource *ZBuffer, ID3D12Resource *HDRSurface, ID3D12Resource *LDRSurface, ID3D12Resource *tonemapReductionBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, const D3D12_GPU_DESCRIPTOR_HANDLE tonemapDescriptorTable, UINT width, UINT height) const
{
	auto cmdLists = cmdListsManager.OnFrameStart();
	GPUWorkSubmission::Prepare();

	GPUWorkSubmission::AppendPipelineStage<false>(Pre, cmdLists.pre, output, rtv, dsv, width, height);

	const function<void (ID3D12GraphicsCommandList2 *target, bool enableRT)> setupRenderOutputCallback =
		[
			rtv, dsv,
			viewport = CD3DX12_VIEWPORT(0.f, 0.f, width, height),
			scissorRect = CD3DX12_RECT(0, 0, width, height)
		](ID3D12GraphicsCommandList2 *cmdList, bool enableRT)
	{
		cmdList->OMSetRenderTargets(enableRT, &rtv, TRUE, &dsv);
		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);
	};
	if (world)
		world->Render(ctx, viewXform, projXform, ZBuffer, dsv, setupRenderOutputCallback);

	GPUWorkSubmission::AppendPipelineStage<false>(Post, cmdLists.post, output, rendertarget, HDRSurface, LDRSurface, tonemapReductionBuffer, tonemapDescriptorTable, width, height);

	GPUWorkSubmission::Run();
	cmdListsManager.OnFrameFinish();
}

void Impl::Viewport::OnFrameFinish() const
{
	if (world)
		world->OnFrameFinish();
}