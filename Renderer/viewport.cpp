#include "stdafx.h"
#include "viewport.hh"
#include "world.hh"
#include "GPU work submission.h"
#include "GPU descriptor heap.h"
#include "offscreen buffers.h"
#include "render passes.h"
#include "shader bytecode.h"
#include "DMA engine.h"
#include "config.h"
#include "CS config.h"
#include "PIX events.h"

namespace Shaders
{
#	include "luminanceTextureReduction.csh"
#	include "luminanceBufferReduction.csh"
#	include "lensFlareVS.csh"
#	include "lensFlareGS.csh"
#	include "lensFlarePS.csh"
#	include "COC_pass.csh"
#	include "DOF_splattingVS.csh"
#	include "DOF_splattingGS.csh"
#	include "DOF_splattingPS.csh"
#	include "bokehComposite.csh"
#	include "brightPass.csh"
#	include "downsample.csh"
#	include "upsampleBlur.csh"
#	include "postprocessFinalComposite.csh"
}

using namespace std;
using namespace numbers;
using namespace Renderer;
using namespace Math::VectorMath::HLSL;
using WRL::ComPtr;
namespace RenderPipeline = Impl::RenderPipeline;

#include "camera params.hlsli"

extern ComPtr<ID3D12Device4> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);
	
enum struct CameraAdaptation
{
	LumBright,
	LumDark,
	Focus,
};

template<CameraAdaptation>
static constexpr float adaptationRate;

template<>
static constexpr float adaptationRate<CameraAdaptation::LumBright> = 2;

template<>
static constexpr float adaptationRate<CameraAdaptation::LumDark> = 1;

template<>
static constexpr float adaptationRate<CameraAdaptation::Focus> = .7f;

static inline float3 CalculateCamAdaptationLerpFactors(float delta)
{
	// want constexpr in vector math
	static const/*expr*/ float3 cameraAdaptationRate(adaptationRate<CameraAdaptation::LumBright>, adaptationRate<CameraAdaptation::LumDark>, adaptationRate<CameraAdaptation::Focus>);
	return (-cameraAdaptationRate * delta).apply(expf);
}

#pragma region postprocess root sig & PSOs
auto Impl::Viewport::CreatePostprocessRootSigs() -> PostprocessRootSigs
{
	// consider encapsulating desc table in PostprocessDescriptorTableStore
	CD3DX12_ROOT_PARAMETER1 computeRootParams[COMPUTE_ROOT_PARAM_COUNT], gfxRootParams[GFX_ROOT_PARAM_COUNT];
	const D3D12_DESCRIPTOR_RANGE1 computeDescTable[] =
	{
		/*
			descriptor volatile flags below are for hardware out-of-bounds access checking for reduction buffer SRV/UAV
			?: there is D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS flag which seemed as exactly what is needed
				but it fails with message "Unsupported bit-flag set (descriptor range flags 10000)."
				no mention for it in docs other than just presence in https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/ne-d3d12-d3d12_descriptor_range_flags without any description
		*/
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 4),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 4, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 11, 0, 1)
	}, gfxDescTable[] =
	{
		/*
							quite pessimistic but makes DX debug validation happy (alternative option is to try to eliminate split barriers for input HDR surface)
																								^
																								|
																			 _______________________________________
																			|										|*/
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE, Descriptors::PostprocessDescriptorTableStore::SrcSRV),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 6, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, Descriptors::PostprocessDescriptorTableStore::DilatedCOCBufferUAV),
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 4, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE, Descriptors::PostprocessDescriptorTableStore::DOFOpacityBufferSRV)
	};
	computeRootParams[COMPUTE_ROOT_PARAM_DESC_TABLE].InitAsDescriptorTable(size(computeDescTable), computeDescTable);
	computeRootParams[COMPUTE_ROOT_PARAM_CAM_SETTINGS_CBV].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE);	// alternatively can place into desc table with desc static flag
	computeRootParams[COMPUTE_ROOT_PARAM_CAM_SETTINGS_UAV].InitAsUnorderedAccessView(3);
	computeRootParams[COMPUTE_ROOT_PARAM_PERFRAME_DATA_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
	computeRootParams[COMPUTE_ROOT_PARAM_LOD].InitAsConstants(1, 2);
	gfxRootParams[GFX_ROOT_PARAM_DESC_TABLE].InitAsDescriptorTable(size(gfxDescTable), gfxDescTable, D3D12_SHADER_VISIBILITY_VERTEX);
	gfxRootParams[GFX_ROOT_PARAM_CAM_SETTINGS_CBV] = computeRootParams[COMPUTE_ROOT_PARAM_CAM_SETTINGS_CBV];
	const D3D12_STATIC_SAMPLER_DESC samplers[]
	{
		CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,			D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR),
		CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_POINT,					D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR),
		CD3DX12_STATIC_SAMPLER_DESC(2, D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT,	D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR)
	};
	const D3D12_ROOT_SIGNATURE_FLAGS gfxFlags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC
		computeSigDesc(size(computeRootParams), computeRootParams, size(samplers) - 1, samplers),
		gfxSigDesc(size(gfxRootParams), gfxRootParams, size(samplers), samplers, gfxFlags);
	return
	{
		CreateRootSignature(computeSigDesc, L"postprocess compute root signature"),
		CreateRootSignature(gfxSigDesc, L"postprocess gfx root signature")
	};
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateLuminanceTextureReductionPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature = postprocessRootSigs.compute.Get(),
		.CS = ShaderBytecode(Shaders::luminanceTextureReduction),
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
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
		.pRootSignature = postprocessRootSigs.compute.Get(),
		.CS = ShaderBytecode(Shaders::luminanceBufferReduction),
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
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

WRL::ComPtr<ID3D12PipelineState> Renderer::Impl::Viewport::Create_COC_pass_PSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature = postprocessRootSigs.compute.Get(),
		.CS = ShaderBytecode(Shaders::COC_pass),
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"COC pass PSO");
	return PSO;
}

WRL::ComPtr<ID3D12PipelineState> Renderer::Impl::Viewport::Create_DOF_splatting_PSO()
{
	CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = blendDesc.RenderTarget[0].DestBlend = blendDesc.RenderTarget[0].SrcBlendAlpha = blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;

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
		.VS = ShaderBytecode(Shaders::DOF_splattingVS),
		.PS = ShaderBytecode(Shaders::DOF_splattingPS),
		.GS = ShaderBytecode(Shaders::DOF_splattingGS),
		.BlendState = blendDesc,
		.SampleMask = UINT_MAX,
		.RasterizerState = rasterDesc,
		.DepthStencilState = dsDesc,
		.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
		.NumRenderTargets = 2,
		.RTVFormats = { Config::DOFLayersFormat, Config::DOFLayersFormat },
		.DSVFormat = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = { 1 },
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"DOF splatting PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateBokehCompositePSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature = postprocessRootSigs.compute.Get(),
		.CS = ShaderBytecode(Shaders::bokehComposite),
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"DOF & lens flare composite PSO");
	return PSO;
}

ComPtr<ID3D12PipelineState> Impl::Viewport::CreateBrightPassPSO()
{
	const D3D12_COMPUTE_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature = postprocessRootSigs.compute.Get(),
		.CS = ShaderBytecode(Shaders::brightPass),
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
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
		. pRootSignature = postprocessRootSigs.compute.Get(),
		.CS = ShaderBytecode(Shaders::downsample),
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
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
		.pRootSignature = postprocessRootSigs.compute.Get(),
		.CS = ShaderBytecode(Shaders::upsampleBlur),
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
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
		.pRootSignature = postprocessRootSigs.compute.Get(),
		.CS = ShaderBytecode(Shaders::postprocessFinalComposite),
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> PSO;
	CheckHR(device->CreateComputePipelineState(&PSO_desc, IID_PPV_ARGS(PSO.GetAddressOf())));
	NameObject(PSO.Get(), L"postprocess final composite PSO");
	return PSO;
}
#pragma endregion

inline RenderPipeline::PipelineStage Impl::Viewport::Pre(PerViewCmdBuffers::DeferredCmdBuffersProvider cmdListProvider, ID3D12Resource *output, const OffscreenBuffers &offscreenBuffers) const
{
	const auto cmdList = cmdListProvider.Acquire(PerViewCmdBuffers::VIEWPORT_PRE);

	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPre), "viewport pre");
	if (fresh)
	{
		// ClearUnorderedAccessViewFloat() can be used instead but it requires CPU & GPU view handles
		constexpr float initVal = 1;
		const auto cameraSettingsGPUAddress = cameraSettingsBuffer->GetGPUVirtualAddress();
		const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER initParams[] =
		{
			{cameraSettingsGPUAddress + offsetof(CameraParams::Settings, relativeExposure), bit_cast<UINT>(initVal)},
			{cameraSettingsGPUAddress + offsetof(CameraParams::Settings, whitePoint), bit_cast<UINT>(initVal)},
			{cameraSettingsGPUAddress + offsetof(CameraParams::Settings, sensorPlane), bit_cast<UINT>(projParams[0]/*F - focus on infinity*/)},
			{cameraSettingsGPUAddress + offsetof(CameraParams::Settings, exposure), bit_cast<UINT>(/*1 * */CameraParams::normFactor)}
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
			CD3DX12_RESOURCE_BARRIER::Aliasing(NULL, offscreenBuffers.GetPersistentBuffers().ZBuffer.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(NULL, offscreenBuffers.GetWorldAndPostFX1Buffers().HDRInputSurface.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(NULL, offscreenBuffers.GetWorldBuffers().rendertarget.Resource()),
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		};
		cmdList->ResourceBarrier(size(barriers) - !fresh, barriers);
	}
	CheckHR(cmdList->Close());
	return cmdList;
}

inline RenderPipeline::PipelineStage Impl::Viewport::Post(PerViewCmdBuffers::DeferredCmdBuffersProvider cmdListProvider, ID3D12Resource *output, const OffscreenBuffers &offscreenBuffers,
	D3D12_GPU_DESCRIPTOR_HANDLE postprocessDescriptorTable, UINT width, UINT height) const
{
	const auto cmdList = cmdListProvider.Acquire(PerViewCmdBuffers::VIEWPORT_POST, luminanceTextureReductionPSO.Get());

	PIXScopedEvent(cmdList, PIX_COLOR_INDEX(PIXEvents::ViewportPost), "viewport post");

	{
		const auto &bokehAndLumBuffers = offscreenBuffers.GetPostFXBuffers();
		const auto &bokehBuffers = offscreenBuffers.GetPostFX1Buffers();
		typedef remove_reference_t<decltype(bokehAndLumBuffers)> BokehAndLumBuffers;
		typedef remove_reference_t<decltype(bokehBuffers)> BokehBuffers;
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPersistentBuffers().ZBuffer.Resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0),	// don't split to enable D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetWorldAndPostFX1Buffers().HDRInputSurface.Resource(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Aliasing(offscreenBuffers.GetNestingBuffer(&BokehAndLumBuffers::HDRCompositeSurface), bokehAndLumBuffers.HDRCompositeSurface.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(offscreenBuffers.GetNestingBuffer(&BokehBuffers::DOFOpacityBuffer), bokehBuffers.DOFOpacityBuffer.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(offscreenBuffers.GetNestingBuffer(&BokehBuffers::COCBuffer), bokehBuffers.COCBuffer.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(offscreenBuffers.GetNestingBuffer(&BokehBuffers::dilatedCOCBuffer), bokehBuffers.dilatedCOCBuffer.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(offscreenBuffers.GetNestingBuffer(&BokehBuffers::halfresDOFSurface), bokehBuffers.halfresDOFSurface.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(offscreenBuffers.GetNestingBuffer(&BokehBuffers::DOFLayers), bokehBuffers.DOFLayers.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(offscreenBuffers.GetNestingBuffer(&BokehBuffers::lensFlareSurface), bokehBuffers.lensFlareSurface.Resource())
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// bind postprocess resources & setup gfx state
	const auto cameraSettingsBufferGPUAddress = cameraSettingsBuffer->GetGPUVirtualAddress();

	{
		cmdList->SetDescriptorHeaps(1, Descriptors::GPUDescriptorHeap::GetHeap().GetAddressOf());

		// compute
		cmdList->SetComputeRootSignature(postprocessRootSigs.compute.Get());
		cmdList->SetComputeRootDescriptorTable(COMPUTE_ROOT_PARAM_DESC_TABLE, postprocessDescriptorTable);
		cmdList->SetComputeRootConstantBufferView(COMPUTE_ROOT_PARAM_CAM_SETTINGS_CBV, cameraSettingsBufferGPUAddress);
		cmdList->SetComputeRootUnorderedAccessView(COMPUTE_ROOT_PARAM_CAM_SETTINGS_UAV, cameraSettingsBufferGPUAddress);
		cmdList->SetComputeRootConstantBufferView(COMPUTE_ROOT_PARAM_PERFRAME_DATA_CBV, world->GetCurFrameGPUDataPtr());

		// gfx
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		const CD3DX12_RECT halfresRTRect(0, 0, (width + 1) / 2, (height + 1) / 2);
		const CD3DX12_VIEWPORT halfresViewport(halfresRTRect.left, halfresRTRect.top, halfresRTRect.right, halfresRTRect.bottom);
		cmdList->RSSetViewports(1, &halfresViewport);
		cmdList->RSSetScissorRects(1, &halfresRTRect);
		cmdList->SetGraphicsRootSignature(postprocessRootSigs.gfx.Get());
		cmdList->SetGraphicsRootDescriptorTable(GFX_ROOT_PARAM_DESC_TABLE, postprocessDescriptorTable);
		cmdList->SetGraphicsRootConstantBufferView(GFX_ROOT_PARAM_CAM_SETTINGS_CBV, cameraSettingsBufferGPUAddress);
	}

	// initial texture reduction (PSO set during cmd list creation/reset)
	const auto luminanceReductionTexDispatchSize = CSConfig::LuminanceReduction::TexturePass::DispatchSize({ width, height });
	cmdList->Dispatch(luminanceReductionTexDispatchSize.x, luminanceReductionTexDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetLuminanceReductionBuffer().Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
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
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetLuminanceReductionBuffer().Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(cameraSettingsBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	const auto ImageDispatchSize = [width, height](UINT lod) noexcept
	{
		return (uint2(width >> lod, height >> lod) + CSConfig::ImageProcessing::blockSize - 1) / CSConfig::ImageProcessing::blockSize;
	};

	const auto fullResDispatchSize = ImageDispatchSize(0);
	const auto halfResDispatchSize = ImageDispatchSize(1);	// floor

	// COC pass
	cmdList->SetPipelineState(COC_pass_PSO.Get());
	cmdList->Dispatch(fullResDispatchSize.x, fullResDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPersistentBuffers().ZBuffer.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE, 0, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().DOFOpacityBuffer.Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().COCBuffer.Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// lens flare combined with DOF downsample & pixel-sized splats
	{
		const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmdList->SetPipelineState(lensFlarePSO.Get());
		const D3D12_RENDER_PASS_RENDER_TARGET_DESC rtDesc
		{
			offscreenBuffers.GetRTV(offscreenBuffers.LENS_FLARE_RTV),
			{
				.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR,
				.Clear{ {.Format = Config::HDRFormat, .Color{} } }
			},
			{
				.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
			}
		};
		cmdList->ClearUnorderedAccessViewFloat(CD3DX12_GPU_DESCRIPTOR_HANDLE(postprocessDescriptorTable, Descriptors::PostprocessDescriptorTableStore::DOFLayersUAV, descriptorSize),
			offscreenBuffers.GetPostprocessCPUDescriptorTableStore().GetDescriptor(Descriptors::PostprocessDescriptorTableStore::DOFLayersUAV),
			offscreenBuffers.GetPostFX1Buffers().DOFLayers.Resource(),
			rtDesc.BeginningAccess.Clear.ClearValue.Color, 0, NULL);
		cmdList->BeginRenderPass(1, &rtDesc, NULL, D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES);
		cmdList->DrawInstanced(((width + 1) / 2) * ((height + 1) / 2), 1, 0, 0);
		cmdList->EndRenderPass();
	}

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().lensFlareSurface.Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().dilatedCOCBuffer.Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().halfresDOFSurface.Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().DOFLayers.Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// DOF splatting pass
	{
		// no clears/discards for RT, preserve pre/post => don't use render pass here
		cmdList->SetPipelineState(DOF_splatting_PSO.Get());
		const auto DOFLayersRTVs = offscreenBuffers.GetRTV(offscreenBuffers.DOF_LAYERS_RTVs);
		cmdList->OMSetRenderTargets(2, &DOFLayersRTVs, TRUE, NULL);
		cmdList->DrawInstanced(((width + 1) / 2) * ((height + 1) / 2), 1, 0, 0);
	}

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().lensFlareSurface.Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().COCBuffer.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().halfresDOFSurface.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().DOFLayers.Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// DOF & lens flare composite pass
	cmdList->SetPipelineState(bokehCompositePSO.Get());
	cmdList->Dispatch(fullResDispatchSize.x, fullResDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetWorldAndPostFX1Buffers().HDRInputSurface.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFXBuffers().HDRCompositeSurface.Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().lensFlareSurface.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().DOFOpacityBuffer.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().dilatedCOCBuffer.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().DOFLayers.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Aliasing(NULL, offscreenBuffers.GetPostFX2Buffers().bloomUpChain.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(NULL, offscreenBuffers.GetPostFX2Buffers().bloomDownChain.Resource()),
			CD3DX12_RESOURCE_BARRIER::Aliasing(NULL, offscreenBuffers.GetPostFX2Buffers().LDRSurface.Resource())
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// bloom scene downsample + bright pass
	{
		cmdList->SetPipelineState(brightPassPSO.Get());
		cmdList->Dispatch(halfResDispatchSize.x, halfResDispatchSize.y, 1);
		ID3D12Resource *const bloomUpChain = offscreenBuffers.GetPostFX2Buffers().bloomUpChain.Resource(), *const bloomDownChain = offscreenBuffers.GetPostFX2Buffers().bloomDownChain.Resource();

		{
			const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(bloomDownChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0);
			cmdList->ResourceBarrier(1, &barrier);
		}

		const UINT bloomUpChainLen = bloomUpChain->GetDesc().MipLevels, bloomDownChainLen = bloomDownChain->GetDesc().MipLevels;
		UINT lod = 0;

		// bloom downsample chain
		for (cmdList->SetPipelineState(bloomDownsamplePSO.Get()); lod < bloomDownChainLen; lod++)
		{
			cmdList->SetComputeRoot32BitConstant(COMPUTE_ROOT_PARAM_LOD, lod, 0);
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
			cmdList->SetComputeRoot32BitConstant(COMPUTE_ROOT_PARAM_LOD, lod - 1, 0);
			const auto dispatchSize = ImageDispatchSize(lod/*halfres - dest offset*/);
			cmdList->Dispatch(dispatchSize.x, dispatchSize.y, 1);

			{
				const D3D12_RESOURCE_BARRIER barriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, lod, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
					CD3DX12_RESOURCE_BARRIER::Transition(bloomUpChain, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, lod - 1)
				};
				cmdList->ResourceBarrier(size(barriers), barriers);
			}
		}
	}

	// postprocess main final composite pass
	cmdList->SetPipelineState(postrpocessFinalCompositePSO.Get());
	cmdList->Dispatch(fullResDispatchSize.x, fullResDispatchSize.y, 1);

	{
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFXBuffers().HDRCompositeSurface.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX2Buffers().LDRSurface.Resource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX2Buffers().bloomUpChain.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	// copy to output
	{
		const CD3DX12_TEXTURE_COPY_LOCATION copyDst(output, 0), copySrc(offscreenBuffers.GetPostFX2Buffers().LDRSurface.Resource(), 0);
		cmdList->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, NULL);
	}

	{
		/*
			finish all barriers here as last action
			it can be delayed even further - until next frame (potentially more efficient - less chance of GPU idle)
			but resource creation tracking needed for it (do not issue barrier for fresh resource)
		*/
		const D3D12_RESOURCE_BARRIER barriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPersistentBuffers().ZBuffer.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE, 0, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetWorldAndPostFX1Buffers().HDRInputSurface.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFXBuffers().HDRCompositeSurface.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX2Buffers().LDRSurface.Resource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),	// !: use split barrier
			CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetLuminanceReductionBuffer().Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX2Buffers().bloomUpChain.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX2Buffers().bloomDownChain.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().lensFlareSurface.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().DOFOpacityBuffer.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().COCBuffer.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().dilatedCOCBuffer.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().halfresDOFSurface.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY),
			CD3DX12_RESOURCE_BARRIER::Transition(offscreenBuffers.GetPostFX1Buffers().DOFLayers.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
		};
		cmdList->ResourceBarrier(size(barriers), barriers);
	}

	CheckHR(cmdList->Close());
	return cmdList;
}

Impl::Viewport::Viewport(shared_ptr<const Renderer::World> world) : world(move(world)), viewXform(), projXform()
{
	assert(this->world);

	viewXform[0][0] = viewXform[1][1] = viewXform[2][2] = projXform[0][0] = projXform[1][1] = projXform[2][3] = 1.f;
	SetProjectionTransform(pi / 2, 1.);

	/*
		create camera settings buffer
		very small size - consider sharing it with other viewport persistent data
	*/
	const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(CameraParams::Settings), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	CheckHR(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
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

	projParams[0] = zn * .5/*take half znear for F for now, consider configurable factor instead or derive from physical sensor size*/;
	projParams[1] = 1 / zn;
	projParams[2] = projParams[1] - 1 / zf;
}

void Impl::Viewport::UpdateAspect(double invAspect)
{
	projXform[0][0] = projXform[1][1] * invAspect;
}

void Impl::Viewport::Render(ID3D12Resource *output, const class OffscreenBuffers &offscreenBuffers, UINT width, UINT height) const
{
	// fill GPU descriptor heap
	const auto perFrameDescriptorTables = Descriptors::GPUDescriptorHeap::StreamPerFrameDescriptorTables(world->sky, offscreenBuffers.GetPostprocessCPUDescriptorTableStore());

	// time
	using namespace chrono;
	const Clock::time_point curTime = Clock::now();
	const float delta = duration_cast<duration<float>>(curTime - time).count();
	time = curTime;

	auto viewCmdBuffersProvider = viewCmdBuffersManager.OnFrameStart();
	GPUWorkSubmission::Prepare();

	GPUWorkSubmission::AppendPipelineStage<false>(&Viewport::Pre, this, viewCmdBuffersProvider, output, cref(offscreenBuffers));

	const RenderPipeline::RenderPasses::PipelineROPTargets ROPTargets(offscreenBuffers.GetWorldBuffers().rendertarget.Resource(), offscreenBuffers.GetRTV(offscreenBuffers.SCENE_RTV), offscreenBuffers.GetPersistentBuffers().ZBuffer.Resource(), offscreenBuffers.GetDSV(), offscreenBuffers.GetWorldAndPostFX1Buffers().HDRInputSurface.Resource(), width, height);
	world->Render(ctx, viewXform, projXform, projParams, CalculateCamAdaptationLerpFactors(delta), cameraSettingsBuffer->GetGPUVirtualAddress(), D3D12_GPU_DESCRIPTOR_HANDLE{ perFrameDescriptorTables.skybox }, viewCmdBuffersProvider, ROPTargets);

	GPUWorkSubmission::AppendPipelineStage<false>(&Viewport::Post, this, viewCmdBuffersProvider, output, cref(offscreenBuffers), D3D12_GPU_DESCRIPTOR_HANDLE{ perFrameDescriptorTables.postprocess }, width, height);

	// defer as much as possible in order to reduce chances waiting to be inserted in GFX queue (DMA queue can progress enough by this point)
	DMA::Sync();

	GPUWorkSubmission::Run();
	fresh = false;
	viewCmdBuffersManager.OnFrameFinish();
}

void Impl::Viewport::OnFrameFinish() const
{
	world->OnFrameFinish();
}