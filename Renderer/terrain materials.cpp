#include "stdafx.h"
#include "terrain materials.hh"
#include "texture.hh"
#include "GPU texture sampler table.h"
#include "shader bytecode.h"
#include "config.h"
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
}

using namespace std;
using namespace Renderer::TerrainMaterials;
using WRL::ComPtr;
using Misc::AllocatorProxy;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);

// !: descriptor tables specify DATA_STATIC flag so textures must be loaded upfront

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

#pragma region Interface
inline void Impl::Interface::FillRootParams(CD3DX12_ROOT_PARAMETER1 rootParams[])
{
	rootParams[ROOT_PARAM_PER_FRAME_DATA_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
	rootParams[ROOT_PARAM_TONEMAP_PARAMS_CBV].InitAsConstantBufferView(1, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
}

Impl::Interface::Interface(const ComPtr<ID3D12RootSignature> &rootSig, const ComPtr<ID3D12PipelineState> &PSO, UINT color) :
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

#pragma region Flat
ComPtr<ID3D12RootSignature> Flat::CreateRootSig()
{
	CD3DX12_ROOT_PARAMETER1 rootParams[FLAT_ROOT_PARAM_COUNT];
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		rootSig.Get(),									// root signature
		ShaderBytecode(Shaders::vectorLayerVS),			// VS
		ShaderBytecode(Shaders::vectorLayerFlatPS),		// PS
		{},												// DS
		{},												// HS
		{},												// GS
		{},												// SO
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),				// blend
		UINT_MAX,										// sample mask
		rasterDesc,										// rasterizer
		dsDesc,											// depth stencil
		{ VB_decl, size(VB_decl) },						// IA
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,	// restart primtive
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,			// primitive topology
		1,												// render targets
		{ Config::HDRFormat },							// RT formats
		Config::ZFormat,								// depth stencil format
		Config::MSAA(),									// MSAA
		0,												// node mask
		{},												// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE					// flags
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain flat material PSO");
	return move(result);
}

inline void Flat::FillRootParams(CD3DX12_ROOT_PARAMETER1 rootParams[])
{
	Interface::FillRootParams(rootParams);
	rootParams[ROOT_PARAM_ALBEDO].InitAsConstants(3, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
}

static inline BYTE float2BYTE(float val) noexcept
{
	return val * numeric_limits<BYTE>::max();
};

#if defined _MSC_VER && _MSC_VER <= 1920
Flat::Flat(tag, const float (&albedo)[3], const ComPtr<ID3D12RootSignature> &rootSig, const ComPtr<ID3D12PipelineState> &PSO) :
#else
Flat::Flat(const float (&albedo)[3], const ComPtr<ID3D12RootSignature> &rootSig, const ComPtr<ID3D12PipelineState> &PSO) :
#endif
	Interface(rootSig, PSO, PIX_COLOR(float2BYTE(albedo[0]), float2BYTE(albedo[1]), float2BYTE(albedo[2]))),
	albedo{ albedo[0], albedo[1], albedo[2] }
{
}

shared_ptr<Interface> Flat::Make(const float (&albedo)[3])
{
#if defined _MSC_VER && _MSC_VER <= 1920
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
	// desc tables lifetime must last up to create call so if it gets filled in helper function (FillRootParams) it mat be static
	CD3DX12_ROOT_PARAMETER1 rootParams[TEXTURED_ROOT_PARAM_COUNT];
	Flat::FillRootParams(rootParams);
	const CD3DX12_DESCRIPTOR_RANGE1 descTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	rootParams[ROOT_PARAM_TEXTURE_DESC_TABLE].InitAsDescriptorTable(1, &descTable, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_SAMPLER_DESC_TABLE] = Impl::Descriptors::TextureSampers::GetDescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_TEXTURE_SCALE].InitAsConstants(1, 0, 2, D3D12_SHADER_VISIBILITY_VERTEX);
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		rootSig.Get(),									// root signature
		ShaderBytecode(Shaders::vectorLayerVS_UV),		// VS
		ShaderBytecode(Shaders::vectorLayerTexPS),		// PS
		{},												// DS
		{},												// HS
		{},												// GS
		{},												// SO
		CD3DX12_BLEND_DESC(D3D12_DEFAULT),				// blend
		UINT_MAX,										// sample mask
		rasterDesc,										// rasterizer
		dsDesc,											// depth stencil
		{ VB_decl, size(VB_decl) },						// IA
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,	// restart primtive
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,			// primitive topology
		1,												// render targets
		{ Config::HDRFormat },							// RT formats
		Config::ZFormat,								// depth stencil format
		Config::MSAA(),									// MSAA
		0,												// node mask
		{},												// cached PSO
		D3D12_PIPELINE_STATE_FLAG_NONE					// flags
	};

	ComPtr<ID3D12PipelineState> result;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), L"terrain textured material PSO");
	return move(result);
}

#if defined _MSC_VER && _MSC_VER <= 1920
Textured::Textured(tag tag, const float (&albedo)[3], const Renderer::Texture &tex, float texScale, const char materialName[]) :
	DescriptorTable(1, materialName), Flat(tag, albedo, rootSig, PSO), tex(tex), texScale(texScale)
#else
Textured::Textured(const float (&albedo)[3], const Texture &tex, float texScale, const char materialName[]) :
	DescriptorTable(1, materialName), Flat(albedo, rootSig, PSO), tex(tex), texScale(texScale)
#endif
{
	if (tex.Usage() != TextureUsage::AlbedoMap)
		throw invalid_argument("Terrain material: incompatible texture usage, albedo map expected.");
	device->CreateShaderResourceView(this->tex.Get(), NULL, GetCPUStage()->GetCPUDescriptorHandleForHeapStart());
}

Textured::~Textured() = default;

#if defined _MSC_VER && _MSC_VER <= 1920
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

void Textured::FinishSetup(ID3D12GraphicsCommandList2 *cmdList) const
{
	using namespace Impl::Descriptors;
	Flat::FinishSetup(cmdList);
	ID3D12DescriptorHeap *const descHeaps[] = { GPUDescriptorHeap::GetHeap().Get(), TextureSampers::GetHeap().Get() };
	cmdList->SetDescriptorHeaps(size(descHeaps), descHeaps);
	cmdList->SetGraphicsRootDescriptorTable(ROOT_PARAM_TEXTURE_DESC_TABLE, { GetGPUDescriptorsAllocation() });
	cmdList->SetGraphicsRootDescriptorTable(ROOT_PARAM_SAMPLER_DESC_TABLE, descHeaps[1]/*samplers heap*/->GetGPUDescriptorHandleForHeapStart());
	cmdList->SetGraphicsRoot32BitConstant(ROOT_PARAM_TEXTURE_SCALE, reinterpret_cast<const UINT &>(texScale), 0);	// passed to D3D12 runtime so 'reinterpret_cast' suitable here
}
#pragma endregion