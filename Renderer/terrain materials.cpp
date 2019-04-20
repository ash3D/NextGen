#include "stdafx.h"
#include "terrain materials.hh"
#include "shader bytecode.h"
#include "config.h"
#ifdef _MSC_VER
#include <codecvt>
#include <locale>
#endif

namespace Shaders
{
#	include "vectorLayerVS.csh"
#	include "vectorLayerPS.csh"
}

using namespace std;
using namespace Renderer::TerrainMaterials;
using WRL::ComPtr;
using Misc::AllocatorProxy;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);

#pragma region DescriptorTable
Impl::DescriptorTable::DescriptorTable(unsigned size, const char materialName[]) : AllocationClient(size)
{
	const D3D12_DESCRIPTOR_HEAP_DESC desc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,		// type
		size,										// count
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE	// GPU visible
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
	rootParams[ROOT_PARAM_PER_FRAME_DATA_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);
	rootParams[ROOT_PARAM_TONEMAP_PARAMS_CBV].InitAsConstantBufferView(1, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_ALBEDO].InitAsConstants(3, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
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
		ShaderBytecode(Shaders::vectorLayerPS),			// PS
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
	return allocate_shared<Flat>(AllocatorPeoxy<Flat>(), albedo);
#endif
}

// 'inline' for (hopefully) devirtualized call from 'Textured', for common vtable dispatch path compiler still have to generate out-of-line body code
inline void Flat::FinishSetup(ID3D12GraphicsCommandList2 *cmdList) const
{
	cmdList->SetGraphicsRoot32BitConstants(ROOT_PARAM_ALBEDO, size(albedo), albedo, 0);
}
#pragma endregion

#pragma region Textured
void Textured::FinishSetup(ID3D12GraphicsCommandList2 *cmdList) const
{
	Flat::FinishSetup(cmdList);
	cmdList->SetGraphicsRootDescriptorTable(ROOT_PARAM_TEXTURE_DESC_TABLE, { GetGPUDescriptorsAllocation() });
}
#pragma endregion