#include "stdafx.h"
#include "viewport.hh"
#include "world.hh"
#include "GPU work submission.h"
#include "GPU descriptor heap.h"
#include "render passes.h"
#include "shader bytecode.h"
#include "DMA engine.h"
#include "config.h"
#include "PIX events.h"
#include "CS config.h"
#include "camera params.hlsli"

namespace Shaders
{
#	include "luminanceTextureReduction.csh"
#	include "luminanceBufferReduction.csh"
#	include "lensFlareVS.csh"
#	include "lensFlareGS.csh"
#	include "lensFlarePS.csh"
#	include "brightPass.csh"
#	include "downsample.csh"
#	include "upsampleBlur.csh"
#	include "postprocessFinalComposite.csh"
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

static inline float CalculateLumAdaptationLerpFactor(float delta)
{
	static constexpr float adaptationSpeed = 1;

	return exp(-adaptationSpeed * delta);
}

#pragma region cmd buffers
ID3D12GraphicsCommandList4 *Impl::Viewport::DeferredCmdBuffsProvider::Acquire(ComPtr<ID3D12GraphicsCommandList4> &list, const WCHAR listName[], ID3D12PipelineState *PSO)
{
	if (list)
	{
		// reset list
		CheckHR(list->Reset(cmdBuffers.allocator.Get(), PSO));
	}
	else
	{
		// create list
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdBuffers.allocator.Get(), PSO, IID_PPV_ARGS(&list)));
		NameObjectF(list.Get(), L"viewport %p %ls command list [%hu]", viewportPtr, listName, createVersion);
	}

	return list.Get();
}

inline ID3D12GraphicsCommandList4 *Impl::Viewport::DeferredCmdBuffsProvider::AcquirePre()
{
	return Acquire(cmdBuffers.pre, L"pre", NULL);
}

inline ID3D12GraphicsCommandList4 *Impl::Viewport::DeferredCmdBuffsProvider::AcquirePost()
{
	return Acquire(cmdBuffers.post, L"post", luminanceTextureReductionPSO.Get());
}

// result valid until call to 'OnFrameFinish()'
auto Impl::Viewport::CmdBuffsManager::OnFrameStart() -> DeferredCmdBuffsProvider
{
	FrameVersioning::OnFrameStart();
	PrePostCmdBuffs &cmdBuffers = GetCurFrameDataVersion();

	if (cmdBuffers.allocator)
	{
		// reset allocator
		CheckHR(cmdBuffers.allocator->Reset());

		return DeferredCmdBuffsProvider{ cmdBuffers };
	}
	else
	{
#if ENABLE_NONSTDLAYOUT_OFFSETOF
		const void *const viewportPtr = reinterpret_cast<const std::byte *>(this) - offsetof(Viewport, cmdBuffsManager);
#else
		const void *const viewportPtr = this;
#endif
		const unsigned short createVersion = GetFrameLatency() - 1;

		// create allocator
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdBuffers.allocator)));
		NameObjectF(cmdBuffers.allocator.Get(), L"viewport %p command allocator [%hu]", viewportPtr, createVersion);

		return DeferredCmdBuffsProvider{ cmdBuffers, viewportPtr, createVersion };
	}
}
#pragma endregion

#pragma region postprocess root sig & PSOs
auto Impl::Viewport::CreatePostprocessRootSigs() -> PostprocessRootSigs
{
	// consider encapsulating desc table in PostprocessDescriptorTableStore
	CD3DX12_ROOT_PARAMETER1 computeRootParams[COMPUTE_ROOT_PARAM_COUNT], gfxRootParams[GFX_ROOT_PARAM_COUNT];
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
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 11, 0, 1)
	};
	computeRootParams[COMPUTE_ROOT_PARAM_DESC_TABLE].InitAsDescriptorTable(size(descTable), descTable);
	computeRootParams[COMPUTE_ROOT_PARAM_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);	// alternatively can place into desc table with desc static flag
	computeRootParams[COMPUTE_ROOT_PARAM_UAV].InitAsUnorderedAccessView(2);
	computeRootParams[COMPUTE_ROOT_PARAM_PUSH_CONST].InitAsConstants(1, 1);
	(gfxRootParams[GFX_ROOT_PARAM_DESC_TABLE] = computeRootParams[COMPUTE_ROOT_PARAM_DESC_TABLE]).ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	gfxRootParams[GFX_ROOT_PARAM_CBV] = computeRootParams[COMPUTE_ROOT_PARAM_CBV];
	const CD3DX12_STATIC_SAMPLER_DESC resampleTapFilter(0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
	const D3D12_ROOT_SIGNATURE_FLAGS gfxFlags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS		|
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS	|
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS	|
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeSigDesc(size(computeRootParams), computeRootParams, 1, &resampleTapFilter), gfxSigDesc(size(gfxRootParams), gfxRootParams, 1, &resampleTapFilter, gfxFlags);
	return { CreateRootSignature(computeSigDesc, L"postprocess compute root signature"), CreateRootSignature(gfxSigDesc, L"postprocess gfx root signature") };
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateLuminanceTextureReductionPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSigs.compute.Get(),					// root signature
		ShaderBytecode(Shaders::luminanceTextureReduction),	// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"luminance texture reduction PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateLuminanceBufferReductionPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSigs.compute.Get(),					// root signature
		ShaderBytecode(Shaders::luminanceBufferReduction),	// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"luminance buffer reduction PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateLensFlarePSO()
{
	CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	// alpha not used, zero it out hoping for better render target compression
	blendDesc.RenderTarget[0].SrcBlendAlpha = blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_NONE,
		FALSE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		FALSE,										// depth clip
		FALSE,										// MSAA
		FALSE,										// AA line
		0,											// force sample count
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	);

	const CD3DX12_DEPTH_STENCIL_DESC dsDesc
	(
		FALSE,																									// depth
		D3D12_DEPTH_WRITE_MASK_ZERO,
		D3D12_COMPARISON_FUNC_ALWAYS,
		FALSE,																									// stencil
		D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
		D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS,		// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS		// back
	);

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature = postprocessRootSigs.gfx.Get(),
		.VS = ShaderBytecode(Shaders::lensFlareVS),
		.PS = ShaderBytecode(Shaders::lensFlarePS),
		.GS = ShaderBytecode(Shaders::lensFlareGS),
		.BlendState = blendDesc,
		.SampleMask = UINT_MAX,
		.RasterizerState = rasterDesc,
		.DepthStencilState = dsDesc,
		.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
		.NumRenderTargets = 1,
		.RTVFormats = { Config::HDRFormat },
		.DSVFormat = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = { 1 },
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"lens flare PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateBrightPassPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSigs.compute.Get(),					// root signature
		ShaderBytecode(Shaders::brightPass),				// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"bloom bright pass PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateBloomDownsmplePSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSigs.compute.Get(),					// root signature
		ShaderBytecode(Shaders::downsample),				// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"bloom downsample PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateBloomUpsmpleBlurPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSigs.compute.Get(),					// root signature
		ShaderBytecode(Shaders::upsampleBlur),				// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"bloom upsample blur PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreatePostprocessFinalCompositePSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		postprocessRootSigs.compute.Get(),					// root signature
		ShaderBytecode(Shaders::postprocessFinalComposite),	// CS
		0,													// node mask
		{},													// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE						// flags
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"postprocess final composite PSO");
	return PSO;
}
#pragma endregion

inline RenderPipeline::PipelineStage Impl::Viewport::Pre(DeferredCmdBuffsProvider cmdListProvider, ID3D12Resource *output, D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv) const
{
	const auto cmdList = cmdListProvider.AcquirePre();

	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPre), "viewport pre");
	if (fresh)
	{
		// ClearUnorderedAccessViewFloat() can be used instead but it requires CPU & GPU view handles
		constexpr float initVal = 1;
		const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER initParams[] =
		{
			{cameraSettingsBuffer->GetGPUVirtualAddress(), reinterpret_cast<const UINT &>(initVal)/*strict aliasing rule violation, use C++20 bit_cast instead*/},
			{initParams[0].Dest + sizeof(float), initParams[0].Value},
			{initParams[1].Dest + sizeof(float), reinterpret_cast<const UINT &>(/*1 * */CameraParams::normFactor)/*strict aliasing rule violation, use C++20 bit_cast instead*/}
		};
		cmdList->WriteBufferImmediate(size(initParams), initParams, NULL);
	}
	{
		/*
			could potentially use split barrier for cameraSettingsBuffer to overlap with occlusion culling but it would require conditional finish barrier in world render
			anyway barrier happens only once at viewport creation
		*/
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		};
		cmdList->ResourceBarrier(1 + fresh, barriers);
	}
	CheckHR(cmdList->Close());
	return cmdList;
}

inline RenderPipeline::PipelineStage Impl::Viewport::Post(DeferredCmdBuffsProvider cmdListProvider, ID3D12Resource *output, ID3D12Resource *rendertarget, ID3D12Resource *HDRSurface, ID3D12Resource *LDRSurface,
	ID3D12Resource *lensFlareSurface, ID3D12Resource *bloomUpChain, ID3D12Resource *bloomDownChain, ID3D12Resource *luminanceReductionBuffer,
	D3D12_CPU_DESCRIPTOR_HANDLE rtvLensFlare, D3D12_GPU_DESCRIPTOR_HANDLE postprocessDescriptorTable, float lumAdaptationLerpFactor, UINT width, UINT height) const
{
	const auto cmdList = cmdListProvider.AcquirePost();

	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPost), "viewport post");

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(HDRSurface, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// bind postprocess resources
	{
		const auto cameraSettingsBufferGPUAddress = cameraSettingsBuffer->GetGPUVirtualAddress();
		cmdList->SetDescriptorHeaps(1, Descriptors::GPUDescriptorHeap::GetHeap().GetAddressOf());

		// compute
		cmdList->SetComputeRootSignature(postprocessRootSigs.compute.Get());
		cmdList->SetComputeRootDescriptorTable(COMPUTE_ROOT_PARAM_DESC_TABLE, postprocessDescriptorTable);
		cmdList->SetComputeRootConstantBufferView(COMPUTE_ROOT_PARAM_CBV, cameraSettingsBufferGPUAddress);
		cmdList->SetComputeRootUnorderedAccessView(COMPUTE_ROOT_PARAM_UAV, cameraSettingsBufferGPUAddress);
		cmdList->SetComputeRoot32BitConstant(COMPUTE_ROOT_PARAM_PUSH_CONST, reinterpret_cast<const UINT &>(lumAdaptationLerpFactor)/*use C++20 bit_cast instead*/, 0);

		// gfx
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		cmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(lensFlareSurface));
		cmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, width / 2, height / 2));
		cmdList->SetGraphicsRootSignature(postprocessRootSigs.gfx.Get());
		cmdList->SetGraphicsRootDescriptorTable(GFX_ROOT_PARAM_DESC_TABLE, postprocessDescriptorTable);
		cmdList->SetGraphicsRootConstantBufferView(GFX_ROOT_PARAM_CBV, cameraSettingsBufferGPUAddress);
	}

	// initial texture reduction (PSO set during cmd list creation/reset)
	const auto luminanceReductionTexDispatchSize = CSConfig::LuminanceReduction::TexturePass::DispatchSize({ width, height });
	cmdList->Dispatch(luminanceReductionTexDispatchSize.x, luminanceReductionTexDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(luminanceReductionBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// final buffer reduction
	cmdList->SetPipelineState(luminanceBufferReductionPSO.Get());
	cmdList->Dispatch(1, 1, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(luminanceReductionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	const auto ImageDispatchSize = [width, height](UINT lod) noexcept
	{
		return (Math::VectorMath::HLSL::uint2(width >> lod, height >> lod) + CSConfig::ImageProcessing::blockSize - 1) / CSConfig::ImageProcessing::blockSize;
	};

	const auto halfResDispatchSize = ImageDispatchSize(1);

	// lens flare combined with decode 2 halfres
	{
		cmdList->SetPipelineState(lensFlarePSO.Get());
		const D3D12_RENDER_PASS_RENDER_TARGET_DESC rtDesc
		{
			rtvLensFlare,
			{
				.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR,
				.Clear{ {.Format = Config::HDRFormat, .Color{} } }
			},
			{
				.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
			}
		};
		cmdList->BeginRenderPass(1, &rtDesc, NULL, D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES);
		cmdList->DrawInstanced((width / 2) * (height / 2), 1, 0, 0);
		cmdList->EndRenderPass();
	}

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0),
			CD3DX12_RESOURCE_BARRIER::Transition(lensFlareSurface, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// bloom bright pass
	cmdList->SetPipelineState(brightPassPSO.Get());
	cmdList->Dispatch(halfResDispatchSize.x, halfResDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(bloomDownChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	const UINT bloomUpChainLen = bloomUpChain->GetDesc().MipLevels, bloomDownChainLen = bloomDownChain->GetDesc().MipLevels;
	UINT lod = 0;

	// bloom downsample chain
	for (cmdList->SetPipelineState(bloomDownsamplePSO.Get()); lod < bloomDownChainLen; lod++)
	{
		cmdList->SetComputeRoot32BitConstant(COMPUTE_ROOT_PARAM_PUSH_CONST, lod, 0);
		const auto dispatchSize = ImageDispatchSize(lod + 2/*halfres + dest offset*/);
		cmdList->Dispatch(dispatchSize.x, dispatchSize.y, 1);

		{
			const D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(bloomDownChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, lod, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
				CD3DX12_RESOURCE_BARRIER::Transition(lod + 1 < bloomDownChainLen ? bloomDownChain : bloomUpChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, lod + 1)
			};
			cmdList->ResourceBarrier(size(barriers), barriers);
		}
	}

	// bloom blur + upsample chain
	for (cmdList->SetPipelineState(bloomUpsampleBlurPSO.Get()); lod > 0; lod--)
	{
		cmdList->SetComputeRoot32BitConstant(COMPUTE_ROOT_PARAM_PUSH_CONST, lod - 1, 0);
		const auto dispatchSize = ImageDispatchSize(lod/*halfres - dest offset*/);
		cmdList->Dispatch(dispatchSize.x, dispatchSize.y, 1);

		{
			const D3D12_RESOURCE_BARRIER barriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, lod, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
				CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, lod - 1),
				CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
			};
			cmdList->ResourceBarrier(2 + (lod == 2), barriers);
		}
	}

	// postprocess main final composite pass
	cmdList->SetPipelineState(postrpocessFinalCompositePSO.Get());
	const auto fullResDispatchSize = ImageDispatchSize(0);
	cmdList->Dispatch(fullResDispatchSize.x, fullResDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(HDRSurface, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(LDRSurface, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(lensFlareSurface, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// copy to output
	cmdList->CopyTextureRegion(&CD3DX12_TEXTURE_COPY_LOCATION(output, 0), 0, 0, 0, &CD3DX12_TEXTURE_COPY_LOCATION(LDRSurface, 0), NULL);

	{
		/*
			finish all barriers here as last action
			it can be delayed even further - until next frame(potentially more efficient - less chance of GPU idle)
			but resource creation tracking needed for it (do not issue barrier for fresh resource)
		*/
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(HDRSurface, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(LDRSurface, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),	// !: use split barrier
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT),
			CD3DX12_RESOURCE_BARRIER::Transition(luminanceReductionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(bloomDownChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(lensFlareSurface, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	CheckHR(cmdList->Close());
	return cmdList;
}

Impl::Viewport::Viewport(shared_ptr<const Renderer::World> world) : world(move(world)), viewXform(), projXform()
{
	assert(this->world);

	viewXform[0][0] = viewXform[1][1] = viewXform[2][2] = projXform[0][0] = projXform[1][1] = projXform[2][2] = projXform[3][3] = 1.f;

	/*
		create camera settings buffer
		very small size - consider sharing it with other viewport persistent data
	*/
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(float [5]), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_COPY_DEST,
		NULL,
		IID_PPV_ARGS(cameraSettingsBuffer.GetAddressOf())
	));
	NameObjectF(cameraSettingsBuffer.Get(), L"camera settings buffer for viewport %p", static_cast<Renderer::Viewport *>(this));
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

void Impl::Viewport::Render(ID3D12Resource *output, ID3D12Resource *rendertarget, ID3D12Resource *ZBuffer, ID3D12Resource *HDRSurface, ID3D12Resource *LDRSurface,
	ID3D12Resource *lensFlareSurface, ID3D12Resource *bloomUpChain, ID3D12Resource *bloomDownChain, ID3D12Resource *luminanceReductionBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, const D3D12_CPU_DESCRIPTOR_HANDLE rtvLensFlare,
	const D3D12_GPU_DESCRIPTOR_HANDLE postprocessDescriptorTable, UINT width, UINT height) const
{
	// time
	using namespace chrono;
	const Clock::time_point curTime = Clock::now();
	const float delta = duration_cast<duration<float>>(curTime - time).count();
	time = curTime;

	DeferredCmdBuffsProvider cmdBuffsProvider = cmdBuffsManager.OnFrameStart();
	GPUWorkSubmission::Prepare();

	GPUWorkSubmission::AppendPipelineStage<false>(&Viewport::Pre, this, cmdBuffsProvider, output, rtv, dsv);

	const RenderPipeline::RenderPasses::PipelineROPTargets ROPTargets(rendertarget, rtv, backgroundColor, ZBuffer, dsv, 1.f, 0xef, HDRSurface, width, height);
	world->Render(ctx, viewXform, projXform, cameraSettingsBuffer->GetGPUVirtualAddress(), ROPTargets);

	GPUWorkSubmission::AppendPipelineStage<false>(&Viewport::Post, this, cmdBuffsProvider, output, rendertarget, HDRSurface, LDRSurface, lensFlareSurface, bloomUpChain, bloomDownChain, luminanceReductionBuffer,
		rtvLensFlare, postprocessDescriptorTable, CalculateLumAdaptationLerpFactor(delta), width, height);

	// defer as much as possible in order to reduce chances waiting to be inserted in GFX queue (DMA queue can progress enough by this point)
	DMA::Sync();

	GPUWorkSubmission::Run();
	fresh = false;
	cmdBuffsManager.OnFrameFinish();
}

void Impl::Viewport::OnFrameFinish() const
{
	world->OnFrameFinish();
}