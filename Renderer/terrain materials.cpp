#include "stdafx.h"
#include "terrain materials.hh"
#include "texture.hh"
#include "GPU texture sampler tables.h"
#include "fresnel.h"
#include "shader bytecode.h"
#include "config.h"
#include "PIX events.h"
#ifdef _MSC_VER
#include <codecvt>
#include <locale>
#endif

namespace Shaders
{
#	include "vectorLayerVS.csh"
#	include "vectorLayerVS_UV.csh"
#	include "vectorLayerFlatPS.csh"
#	include "vectorLayerTexPS.csh"
#	include "vectorLayerStdPS.csh"
#	include "vectorLayerExtPS.csh"
}

using namespace std;
using namespace Renderer::TerrainMaterials;
using WRL::ComPtr;
using Misc::AllocatorProxy;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);

// !: do not specify DATA_STATIC flag for textures descriptor tables as their state changed after data upload gets finished (COMMON -> SHADER RESOURCE)

#pragma region Interface
inline void Impl::Interface::FillRootParams(CD3DX12_ROOT_PARAMETER1 rootParams[])
{
	rootParams[ROOT_PARAM_PER_FRAME_DATA_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
	rootParams[ROOT_PARAM_TONEMAP_PARAMS_CBV].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
}

Impl::Interface::Interface(UINT color, const ComPtr<ID3D12RootSignature> &rootSig, const ComPtr<ID3D12PipelineState> &PSO) :
	rootSig(rootSig), PSO(PSO), color(color)
{
}

Impl::Interface::~Interface() = default;

void Impl::Interface::Setup(ID3D12GraphicsCommandList2 *cmdList, UINT64 globalGPUBufferPtr, UINT64 tonemapParamsBufferPtr) const
{
	cmdList->SetGraphicsRootSignature(rootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_PER_FRAME_DATA_CBV, globalGPUBufferPtr);
	cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_TONEMAP_PARAMS_CBV, tonemapParamsBufferPtr);
	FinishSetup(cmdList);
}
#pragma endregion

#pragma region DescriptorTable
Impl::DescriptorTable::DescriptorTable(unsigned size, const char materialName[]) : AllocationClient(size)
{
	const D3D12_DESCRIPTOR_HEAP_DESC desc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,		// type
		size,										// count
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE				// GPU invisible
	};
	CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(stage.GetAddressOf())));
#ifdef _MSC_VER
	// same workaround as for terrain layer
	wstring_convert<codecvt_utf8<WCHAR>> converter;
	NameObjectF(stage.Get(), L"terrain material \"%ls\" descriptor table CPU stage", converter.from_bytes(materialName).c_str());
#else
	NameObjectF(stage.Get(), L"terrain material \"%s\" descriptor table CPU stage", materialName);
#endif
}

Impl::DescriptorTable::~DescriptorTable() = default;

void Impl::DescriptorTable::Commit(D3D12_CPU_DESCRIPTOR_HANDLE dst) const
{
	const auto stageDesc = stage->GetDesc();
	device->CopyDescriptorsSimple(stageDesc.NumDescriptors, dst, stage->GetCPUDescriptorHandleForHeapStart(), stageDesc.Type);
}
#pragma endregion

#pragma region TexStuff
template<class Base>
class Impl::TexStuff<Base>::BaseHasFinishSetup
{
	// template to enable SFINAE
	template<class Class>
	static true_type Test(decltype(declval<Class>().Base::FinishSetup(NULL)) *);

	template<class>
	static false_type Test(...);

public:
	static constexpr bool value = decltype(Test<TexStuff>(nullptr))::value;
};

template<class Base>
void Impl::TexStuff<Base>::SetupQuad(ID3D12GraphicsCommandList2 *cmdList, HLSL::float2 quadCenter) const
{
	cmdList->SetGraphicsRoot32BitConstants(ROOT_PARAM_QUAD_TEXGEN_REDUCTION, 2, &(quadCenter * texScale).apply(roundf), 0);
}

// inline for devirtualized call from derived
template<class Base>
inline void Impl::TexStuff<Base>::FinishSetup(ID3D12GraphicsCommandList2 *cmdList) const
{
	using namespace Impl::Descriptors;
	if constexpr (BaseHasFinishSetup::value)
		Base::FinishSetup(cmdList);
	ID3D12DescriptorHeap *const descHeaps[] = { GPUDescriptorHeap::GetHeap().Get(), TextureSamplers::GetHeap().Get() };
	cmdList->SetDescriptorHeaps(size(descHeaps), descHeaps);
	cmdList->SetGraphicsRootDescriptorTable(ROOT_PARAM_TEXTURE_DESC_TABLE, { GetGPUDescriptorsAllocation() });
	cmdList->SetGraphicsRootDescriptorTable(ROOT_PARAM_SAMPLER_DESC_TABLE, TextureSamplers::GetGPUAddress(TextureSamplers::TERRAIN_DESC_TABLE_ID));
	cmdList->SetGraphicsRoot32BitConstant(ROOT_PARAM_TEXTURE_SCALE, reinterpret_cast<const UINT &>(texScale), 0);	// strict aliasing rule violation, use C++20 bit_cast instead
}

template<class Base>
template<typename ExtraArg>
inline Impl::TexStuff<Base>::TexStuff(float texScale, unsigned textureCount, const char materialName[], const ExtraArg &extraArg, const WRL::ComPtr<ID3D12RootSignature> &rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO) :
	DescriptorTable(textureCount, materialName),
	Base(extraArg, rootSig, PSO),
	texScale(texScale)
{
}
#pragma endregion

#pragma region Flat
ComPtr<ID3D12RootSignature> Flat::CreateRootSig()
{
	CD3DX12_ROOT_PARAMETER1 rootParams[ROOT_PARAM_COUNT];
	FillRootParams(rootParams);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"terrain flat material root signature");
}

ComPtr<ID3D12PipelineState> Flat::CreatePSO()
{
	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_NONE,
		FALSE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,										// depth clip
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
		TRUE,																									// stencil
		D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
		D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL,	// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL	// back
	);

	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= rootSig.Get(),
		.VS						= ShaderBytecode(Shaders::vectorLayerVS),
		.PS						= ShaderBytecode(Shaders::vectorLayerFlatPS),
		.BlendState				= CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask				= UINT_MAX,
		.RasterizerState		= rasterDesc,
		.DepthStencilState		= dsDesc,
		.InputLayout			= { VB_decl, size(VB_decl) },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets		= 1,
		.RTVFormats				= { Config::HDRFormat },
		.DSVFormat				= Config::ZFormat,
		.SampleDesc				= Config::MSAA(),
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain flat material PSO");
	return move(result);
}

inline void Flat::FillRootParams(CD3DX12_ROOT_PARAMETER1 rootParams[])
{
	Interface::FillRootParams(rootParams);
	rootParams[ROOT_PARAM_ALBEDO].InitAsConstants(3, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
}

static inline BYTE float2BYTE(float val) noexcept
{
	return val * numeric_limits<BYTE>::max();
};

#if defined _MSC_VER && _MSC_VER <= 1921
inline Flat::Flat(tag, const float(&albedo)[3], const ComPtr<ID3D12RootSignature> &rootSig, const ComPtr<ID3D12PipelineState> &PSO) : Flat(albedo, rootSig, PSO)
{
}
#endif

Flat::Flat(const float (&albedo)[3], const ComPtr<ID3D12RootSignature> &rootSig, const ComPtr<ID3D12PipelineState> &PSO) :
	Interface(PIX_COLOR(float2BYTE(albedo[0]), float2BYTE(albedo[1]), float2BYTE(albedo[2])), rootSig, PSO),
	albedo{ albedo[0], albedo[1], albedo[2] }
{
}

shared_ptr<Interface> Flat::Make(const float (&albedo)[3])
{
#if defined _MSC_VER && _MSC_VER <= 1921
	return make_shared<Flat>(tag(), albedo);
#else
	return allocate_shared<Flat>(AllocatorProxy<Flat>(), albedo);
#endif
}

// 'inline' for (hopefully) devirtualized call from 'Textured', for common vtable dispatch path compiler still have to generate out-of-line body code
inline void Flat::FinishSetup(ID3D12GraphicsCommandList2 *cmdList) const
{
	cmdList->SetGraphicsRoot32BitConstants(ROOT_PARAM_ALBEDO, size(albedo), albedo, 0);
}
#pragma endregion

#pragma region Textured
ComPtr<ID3D12RootSignature> Textured::CreateRootSig()
{
	namespace TextureSamplers = Impl::Descriptors::TextureSamplers;
	// desc tables lifetime must last up to create call so if it gets filled in helper function (FillRootParams) it must be static
	CD3DX12_ROOT_PARAMETER1 rootParams[ROOT_PARAM_COUNT];
	Flat::FillRootParams(rootParams);
	const CD3DX12_DESCRIPTOR_RANGE1 descTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	rootParams[ROOT_PARAM_TEXTURE_DESC_TABLE].InitAsDescriptorTable(1, &descTable, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_SAMPLER_DESC_TABLE] = TextureSamplers::GetDescTable(TextureSamplers::TERRAIN_DESC_TABLE_ID, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_QUAD_TEXGEN_REDUCTION].InitAsConstants(2, 0, 2, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[ROOT_PARAM_TEXTURE_SCALE].InitAsConstants(1, 1, 2, D3D12_SHADER_VISIBILITY_VERTEX);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"terrain textured material root signature");
}

ComPtr<ID3D12PipelineState> Textured::CreatePSO()
{
	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_NONE,
		FALSE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,										// depth clip
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
		TRUE,																									// stencil
		D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
		D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL,	// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL	// back
	);

	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= rootSig.Get(),
		.VS						= ShaderBytecode(Shaders::vectorLayerVS_UV),
		.PS						= ShaderBytecode(Shaders::vectorLayerTexPS),
		.BlendState				= CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask				= UINT_MAX,
		.RasterizerState		= rasterDesc,
		.DepthStencilState		= dsDesc,
		.InputLayout			= { VB_decl, size(VB_decl) },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets		= 1,
		.RTVFormats				= { Config::HDRFormat },
		.DSVFormat				= Config::ZFormat,
		.SampleDesc				= Config::MSAA(),
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain textured material PSO");
	return move(result);
}

// 1 call site
#if defined _MSC_VER && _MSC_VER <= 1921
inline Textured::Textured(tag tag, const float (&albedoFactor)[3], const Renderer::Texture &tex, float texScale, const char materialName[]) :
#else
inline Textured::Textured(const float (&albedoFactor)[3], const Texture &tex, float texScale, const char materialName[]) :
#endif
	TexStuff(texScale, 1, materialName, albedoFactor, rootSig, PSO), tex(tex.Acquire())
{
	if (tex.Usage() != TextureUsage::AlbedoMap)
		throw invalid_argument("Terrain material: incompatible texture usage, albedo map expected.");
	device->CreateShaderResourceView(this->tex.Get(), NULL, GetCPUStage()->GetCPUDescriptorHandleForHeapStart());
}

Textured::~Textured() = default;

#if defined _MSC_VER && _MSC_VER <= 1921
shared_ptr<Interface> Textured::Make(const float (&albedo)[3], const Renderer::Texture &tex, float texScale, const char materialName[])
{
	return make_shared<Textured>(tag(), albedo, tex, texScale, materialName);
}
#else
shared_ptr<Interface> Textured::Make(const float (&albedo)[3], const Texture &tex, float texScale, const char materialName[])
{
	return allocate_shared<Textured>(AllocatorProxy<Flat>(), albedo, tex, texScale, materialName);
}
#endif
#pragma endregion

#pragma region Standard
ComPtr<ID3D12RootSignature> Standard::CreateRootSig()
{
	namespace TextureSamplers = Impl::Descriptors::TextureSamplers;
	// desc tables lifetime must last up to create call so if it gets filled in helper function (FillRootParams) it must be static
	CD3DX12_ROOT_PARAMETER1 rootParams[ROOT_PARAM_COUNT];
	Interface::FillRootParams(rootParams);
	const CD3DX12_DESCRIPTOR_RANGE1 descTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, TEXTURE_COUNT, 0);
	rootParams[ROOT_PARAM_TEXTURE_DESC_TABLE].InitAsDescriptorTable(1, &descTable, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_SAMPLER_DESC_TABLE] = TextureSamplers::GetDescTable(TextureSamplers::TERRAIN_DESC_TABLE_ID, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_QUAD_TEXGEN_REDUCTION].InitAsConstants(2, 0, 2, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[ROOT_PARAM_TEXTURE_SCALE].InitAsConstants(1, 1, 2, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[ROOT_PARAM_F0].InitAsConstants(1, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"terrain standard material root signature");
}

ComPtr<ID3D12PipelineState> Standard::CreatePSO()
{
	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_NONE,
		FALSE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,										// depth clip
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
		TRUE,																									// stencil
		D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
		D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL,	// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL	// back
	);

	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= rootSig.Get(),
		.VS						= ShaderBytecode(Shaders::vectorLayerVS_UV),
		.PS						= ShaderBytecode(Shaders::vectorLayerStdPS),
		.BlendState				= CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask				= UINT_MAX,
		.RasterizerState		= rasterDesc,
		.DepthStencilState		= dsDesc,
		.InputLayout			= { VB_decl, size(VB_decl) },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets		= 1,
		.RTVFormats				= { Config::HDRFormat },
		.DSVFormat				= Config::ZFormat,
		.SampleDesc				= Config::MSAA(),
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain standard material PSO");
	return move(result);
}

// 1 call site
#if defined _MSC_VER && _MSC_VER <= 1921
inline Standard::Standard(tag tag, const Renderer::Texture &albedoMap, const Renderer::Texture &roughnessMap, const Renderer::Texture &normalMap, float texScale, float IOR, const char materialName[]) :
#else
inline Standard::Standard(const Texture &albedoMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, float IOR, const char materialName[]) :
#endif
	TexStuff(texScale, TEXTURE_COUNT, materialName, PIX_COLOR_INDEX(PIXEvents::TerrainMainPass), rootSig, PSO),
	textures{ albedoMap.Acquire(), roughnessMap.Acquire(), normalMap.Acquire() },
	F0(Fresnel::F0(IOR))
{
	// validate textures usage
	if (albedoMap.Usage() != TextureUsage::AlbedoMap)
		throw invalid_argument("Terrain material: incompatible texture usage, albedo map expected.");
	if (roughnessMap.Usage() != TextureUsage::RoughnessMap)
		throw invalid_argument("Terrain material: incompatible texture usage, roughnessMap map expected.");
	if (normalMap.Usage() != TextureUsage::NormalMap)
		throw invalid_argument("Terrain material: incompatible texture usage, normalMap map expected.");

	// fill descriptor heap
	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE descCPUPtr(GetCPUStage()->GetCPUDescriptorHandleForHeapStart());
	for (unsigned i = 0; i < TEXTURE_COUNT; i++, descCPUPtr.Offset(descriptorSize))
		device->CreateShaderResourceView(textures[i].Get(), NULL, descCPUPtr);
}

Standard::~Standard() = default;

#if defined _MSC_VER && _MSC_VER <= 1921
shared_ptr<Interface> __cdecl Standard::Make(const Renderer::Texture &albedoMap, const Renderer::Texture &roughnessMap, const Renderer::Texture &normalMap, float texScale, float IOR, const char materialName[])
{
	return make_shared<Standard>(tag(), albedoMap, roughnessMap, normalMap, texScale, IOR, materialName);
}
#else
shared_ptr<Interface> __cdecl Standard::Make(const Texture &albedoMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, float IOR, const char materialName[])
{
	return allocate_shared<Standard>(AllocatorProxy<Standard>(), albedoMap, roughnessMap, normalMap, texScale, IOR, materialName);
}
#endif

void Standard::FinishSetup(ID3D12GraphicsCommandList2 *cmdList) const
{
	TexStuff::FinishSetup(cmdList);
	cmdList->SetGraphicsRoot32BitConstant(ROOT_PARAM_F0, reinterpret_cast<const UINT &>(F0), 0);	// strict aliasing rule violation, use C++20 bit_cast instead
}
#pragma endregion

#pragma region Extended
ComPtr<ID3D12RootSignature> Extended::CreateRootSig()
{
	namespace TextureSamplers = Impl::Descriptors::TextureSamplers;
	// desc tables lifetime must last up to create call so if it gets filled in helper function (FillRootParams) it must be static
	CD3DX12_ROOT_PARAMETER1 rootParams[ROOT_PARAM_COUNT];
	Interface::FillRootParams(rootParams);
	const CD3DX12_DESCRIPTOR_RANGE1 descTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, TEXTURE_COUNT, 0);
	rootParams[ROOT_PARAM_TEXTURE_DESC_TABLE].InitAsDescriptorTable(1, &descTable, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_SAMPLER_DESC_TABLE] = TextureSamplers::GetDescTable(TextureSamplers::TERRAIN_DESC_TABLE_ID, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_QUAD_TEXGEN_REDUCTION].InitAsConstants(2, 0, 2, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[ROOT_PARAM_TEXTURE_SCALE].InitAsConstants(1, 1, 2, D3D12_SHADER_VISIBILITY_VERTEX);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"terrain extended material root signature");
}

ComPtr<ID3D12PipelineState> Extended::CreatePSO()
{
	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_NONE,
		FALSE,										// front CCW
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,										// depth clip
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
		TRUE,																									// stencil
		D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
		D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL,	// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_NOT_EQUAL	// back
	);

	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= rootSig.Get(),
		.VS						= ShaderBytecode(Shaders::vectorLayerVS_UV),
		.PS						= ShaderBytecode(Shaders::vectorLayerExtPS),
		.BlendState				= CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask				= UINT_MAX,
		.RasterizerState		= rasterDesc,
		.DepthStencilState		= dsDesc,
		.InputLayout			= { VB_decl, size(VB_decl) },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets		= 1,
		.RTVFormats				= { Config::HDRFormat },
		.DSVFormat				= Config::ZFormat,
		.SampleDesc				= Config::MSAA(),
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain extended material PSO");
	return move(result);
}

// 1 call site
#if defined _MSC_VER && _MSC_VER <= 1921
inline Extended::Extended(tag tag, const Renderer::Texture &albedoMap, const Renderer::Texture &fresnelMap, const Renderer::Texture &roughnessMap, const Renderer::Texture &normalMap, float texScale, const char materialName[]) :
#else
inline Extended::Extended(const Texture &albedoMap, const Texture &fresnelMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, const char materialName[]) :
#endif
	TexStuff(texScale, TEXTURE_COUNT, materialName, PIX_COLOR_INDEX(PIXEvents::TerrainMainPass), rootSig, PSO),
	textures{ albedoMap.Acquire(), fresnelMap.Acquire(), roughnessMap.Acquire(), normalMap.Acquire() }
{
	// validate textures usage
	if (albedoMap.Usage() != TextureUsage::AlbedoMap)
		throw invalid_argument("Terrain material: incompatible texture usage, albedo map expected.");
	if (fresnelMap.Usage() != TextureUsage::FresnelMap)
		throw invalid_argument("Terrain material: incompatible texture usage, fresnel map expected.");
	if (roughnessMap.Usage() != TextureUsage::RoughnessMap)
		throw invalid_argument("Terrain material: incompatible texture usage, roughnessMap map expected.");
	if (normalMap.Usage() != TextureUsage::NormalMap)
		throw invalid_argument("Terrain material: incompatible texture usage, normalMap map expected.");

	// fill descriptor heap
	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE descCPUPtr(GetCPUStage()->GetCPUDescriptorHandleForHeapStart());
	for (unsigned i = 0; i < TEXTURE_COUNT; i++, descCPUPtr.Offset(descriptorSize))
		device->CreateShaderResourceView(textures[i].Get(), NULL, descCPUPtr);
}

Extended::~Extended() = default;

#if defined _MSC_VER && _MSC_VER <= 1921
shared_ptr<Interface> __cdecl Extended::Make(const Renderer::Texture &albedoMap, const Renderer::Texture &fresnelMap, const Renderer::Texture &roughnessMap, const Renderer::Texture &normalMap, float texScale, const char materialName[])
{
	return make_shared<Extended>(tag(), albedoMap, fresnelMap, roughnessMap, normalMap, texScale, materialName);
}
#else
shared_ptr<Interface> __cdecl Extended::Make(const Texture &albedoMap, const Texture &fresnelMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, const char materialName[])
{
	return allocate_shared<Extended>(AllocatorProxy<Extended>(), albedoMap, fresnelMap, roughnessMap, normalMap, texScale, materialName);
}
#endif
#pragma endregion