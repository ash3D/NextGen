#include "stdafx.h"
#include "viewport.hh"
#include "world.hh"
#include "texture.hh"	// for pendingg barriers
#include "GPU work submission.h"
#include "GPU descriptor heap.h"
#include "shader bytecode.h"
#include "CB register.h"
#include "config.h"
#include "PIX events.h"
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

static inline float CalculateTonemapParamsLerpFactor(float delta)
{
	static constexpr float adaptationSpeed = 1;

	return exp(-adaptationSpeed * delta);
}

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
		/*
			descriptor volatile flags below are for hardware out-of-bounds access checking for reduction buffer SRV/UAV
			?: there is D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS flag which seemed as exactly what is needed
				but it fails with message "Unsupported bit-flag set (descriptor range flags 10000)."
				no mention for it in docs other than just presence in https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/ne-d3d12-d3d12_descriptor_range_flags without any description
		*/
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE)
	};
	rootParams[ROOT_PARAM_DESC_TABLE].InitAsDescriptorTable(size(descTable), descTable);
	rootParams[ROOT_PARAM_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);	// alternatively can place into desc table with desc static flag
	rootParams[ROOT_PARAM_UAV].InitAsUnorderedAccessView(2);
	rootParams[ROOT_PARAM_LERP_FACTOR].InitAsConstants(1, 1);
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

inline RenderPipeline::PipelineStage Impl::Viewport::Pre(ID3D12GraphicsCommandList2 *cmdList, ID3D12Resource *output, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, UINT width, UINT height) const
{
	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPre), "viewport pre");
	if (fresh)
	{
		// ClearUnorderedAccessViewFloat() can be used instead but it requies CPU & GPU view handles
		constexpr float initVal = 1;
		const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER initParams[] =
		{
			{tonemapParamsBuffer->GetGPUVirtualAddress(), reinterpret_cast<const UINT &>(initVal)/*strict aliasing rule violation, use C++20 bit_cast instead*/},
			{initParams[0].Dest + sizeof(float), initParams[0].Value}
		};
		cmdList->WriteBufferImmediate(size(initParams), initParams, NULL);
	}
	{
		const auto pendingTextureBarriers = Texture::AcquirePendingBarriers();	// have to keep it alive in order to hold ComPtr`s
		vector<D3D12_RESOURCE_BARRIER> barriers;
		barriers.reserve(pendingTextureBarriers.size() + 1 + fresh);
		transform(pendingTextureBarriers.cbegin(), pendingTextureBarriers.cend(), back_inserter(barriers), [](decltype(pendingTextureBarriers)::const_reference texBarrier)
		{
			return CD3DX12_RESOURCE_BARRIER::Transition(texBarrier.first.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATES(texBarrier.second));
		});
		/*
			could potentially use split barrier for pending texture barriers and tonemapParamsBuffer to overlap with occlusion culling but it would require conditional finish barrier in world render
			anyway tonemapParamsBuffer barrier happens only once at viewport creation
		*/
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY));
		if (fresh)
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(tonemapParamsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
		cmdList->ResourceBarrier(barriers.size(), barriers.data());
	}
	cmdList->ClearRenderTargetView(rtv, backgroundColor, 0, NULL);
	cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0xef, 0, NULL);
	CheckHR(cmdList->Close());
	return cmdList;
}

inline RenderPipeline::PipelineStage Impl::Viewport::Post(ID3D12GraphicsCommandList2 *cmdList, ID3D12Resource *output, ID3D12Resource *rendertarget, ID3D12Resource *HDRSurface, ID3D12Resource *LDRSurface, ID3D12Resource *tonemapReductionBuffer, D3D12_GPU_DESCRIPTOR_HANDLE tonemapDescriptorTable, float tonemapLerpFactor, UINT width, UINT height) const
{
	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPost), "viewport post");

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(tonemapParamsBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(rendertarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

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
	{
		const auto tonemapParamsBufferGPUAddress = tonemapParamsBuffer->GetGPUVirtualAddress();
		cmdList->SetDescriptorHeaps(1, Descriptors::GPUDescriptorHeap::GetHeap().GetAddressOf());
		cmdList->SetComputeRootSignature(tonemapRootSig.Get());
		cmdList->SetComputeRootDescriptorTable(ROOT_PARAM_DESC_TABLE, tonemapDescriptorTable);
		cmdList->SetComputeRootConstantBufferView(ROOT_PARAM_CBV, tonemapParamsBufferGPUAddress);
		cmdList->SetComputeRootUnorderedAccessView(ROOT_PARAM_UAV, tonemapParamsBufferGPUAddress);
		cmdList->SetComputeRoot32BitConstant(ROOT_PARAM_LERP_FACTOR, reinterpret_cast<const UINT &>(tonemapLerpFactor)/*use C++20 bit_cast instead*/, 0);
	}

	// initial texture reduction (PSO set during cmd list creation/reset)
	const auto tonemapReductionTexDispatchSize = ReductionTextureConfig::DispatchSize({ width, height });
	cmdList->Dispatch(tonemapReductionTexDispatchSize.x, tonemapReductionTexDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(tonemapReductionBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(tonemapParamsBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// final buffer reduction
	cmdList->SetPipelineState(tonemapBufferReductionPSO.Get());
	cmdList->Dispatch(1, 1, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(tonemapReductionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),	// !: use split barrier
			CD3DX12_RESOURCE_BARRIER::Transition(tonemapParamsBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// tonemapping main pass
	cmdList->SetPipelineState(tonemapPSO.Get());
	cmdList->Dispatch((width + TonemappingConfig::blockSize - 1) / TonemappingConfig::blockSize, (height + TonemappingConfig::blockSize - 1) / TonemappingConfig::blockSize, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(HDRSurface, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST),					// !: use split barrier
			CD3DX12_RESOURCE_BARRIER::Transition(LDRSurface, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
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

	/*
		create tonemap params buffer
		very small size - consider sharing it with other viewport persistent data
	*/
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(Impl::CBRegister::AlignedRow<3>/*or just 'float [3]'?*/), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_COPY_DEST,
		NULL,
		IID_PPV_ARGS(tonemapParamsBuffer.GetAddressOf())
	));
	NameObjectF(tonemapParamsBuffer.Get(), L"tonemap params buffer for viewport %p", static_cast<Renderer::Viewport *>(this));
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
	// time
	using namespace chrono;
	const Clock::time_point curTime = Clock::now();
	const float delta = duration_cast<duration<float>>(curTime - time).count();
	time = curTime;

	auto cmdLists = cmdListsManager.OnFrameStart();
	GPUWorkSubmission::Prepare();

	GPUWorkSubmission::AppendPipelineStage<false>(&Viewport::Pre, this, cmdLists.pre, output, rtv, dsv, width, height);

	if (world)
	{
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
		world->Render(ctx, viewXform, projXform, tonemapParamsBuffer->GetGPUVirtualAddress(), ZBuffer, dsv, setupRenderOutputCallback);
	}

	GPUWorkSubmission::AppendPipelineStage<false>(&Viewport::Post, this, cmdLists.post, output, rendertarget, HDRSurface, LDRSurface, tonemapReductionBuffer, tonemapDescriptorTable, CalculateTonemapParamsLerpFactor(delta), width, height);

	GPUWorkSubmission::Run();
	fresh = false;
	cmdListsManager.OnFrameFinish();
}

void Impl::Viewport::OnFrameFinish() const
{
	if (world)
		world->OnFrameFinish();
}