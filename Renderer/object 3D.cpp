#include "stdafx.h"
#include "object 3D.hh"
#include "tracked resource.inl"
#include "GPU descriptor heap.h"
#include "GPU texture sampler tables.h"
#include "shader bytecode.h"
#include "config.h"
#ifdef _MSC_VER
#include <codecvt>
#include <locale>
#endif

namespace Shaders
{
#	include "object3D_VS.csh"
#	include "object3D_VS_UV.csh"
#	include "object3D_VS_UV_TG.csh"
#	include "object3DFlat_PS.csh"
#	include "object3DTex_PS.csh"
#	include "object3DAlphatest_PS.csh"
#	include "object3DTV_PS.csh"
#	include "object3DGlass_PS.csh"
#	include "object3DBump_PS.csh"
#	include "object3DBumpGlass_PS.csh"
}

using namespace std;
using namespace Renderer;
using namespace HLSL;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

/*
	it seems that INTEL fails to set root constants inside bundle so use cbuffer instead
	consider GPU/driver detection in runtime instead of current fixed compiletime approach
	btw, using static data flag in root signature v1.1 allows driver to place cbuffer content directly in root signature, thus this workaround can have no impact on shader performance
		(although buffer storage still need to be allocated)
*/
#define INTEL_WORKAROUND 1

#if INTEL_WORKAROUND
namespace
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) alignas(D3D12_COMMONSHADER_CONSTANT_BUFFER_PARTIAL_UPDATE_EXTENTS_BYTE_ALIGNMENT) MaterialData
	{
		float3		albedo;
		uint16_t	textureDescriptorTableOffset, samplerDescriptorTableOffset;
		float		TVBrighntess;
	};
}
#endif

struct Impl::Object3D::Context
{
	ID3D12PipelineState *curPSO;
#if !INTEL_WORKAROUND
	// NAN ensures first compare to trigger
	float3 curAlbedo = NAN;
	float curTVBrighntess = NAN;
#endif
};

#pragma region Subobject
struct Impl::Object3D::Subobject
{
	ID3D12PipelineState *PSO;	// no reference tracking required
	AABB<3> aabb;
	union
	{
		unsigned long int vcount, vOffset;
	};
	unsigned long int triOffset;
	unsigned short int tricount;

private:
	unsigned short int textureDescriptorTableOffset;
	float3 albedo;
	union
	{
		float TVBrighntess;
		bool tiled;
	};
#if INTEL_WORKAROUND
	void (Subobject:: *FillMaterialSelector)(volatile MaterialData *dst) const;
#else
	void (Subobject:: *SetupSelector)(ID3D12GraphicsCommandList4 *target, Context &ctx) const;
#endif

public:
	inline Subobject() = default;
	inline Subobject(const AABB<3> &aabb, unsigned long int vcount, unsigned long int triOffset, unsigned short int tricount, ID3D12PipelineState *PSO, const float3 &albedo);
	inline Subobject(const AABB<3> &aabb, unsigned long int vcount, unsigned long int triOffset, unsigned short int tricount, ID3D12PipelineState *PSO, unsigned short int textureDescriptorTableOffset, bool tiled);
	inline Subobject(const AABB<3> &aabb, unsigned long int vcount, unsigned long int triOffset, unsigned short int tricount, ID3D12PipelineState *PSO, const float3 &albedo, float TVBrighntess, unsigned short int textureDescriptorTableOffset);

public:
#if INTEL_WORKAROUND
	inline void FillMaterialCB(volatile MaterialData *dst) const;
#endif
	inline void Setup(ID3D12GraphicsCommandList4 *target, Context &ctx) const;

private:
	inline auto SamplerDescriptorTableOffset() const noexcept;
#if INTEL_WORKAROUND
	void FillAlbedoMaterial(volatile MaterialData *dst) const, FillTexMaterial(volatile MaterialData *dst) const, FillTVMaterial(volatile MaterialData *dst) const;
#else
	void SetupAlbedo(ID3D12GraphicsCommandList4 *target, Context &ctx) const, SetupTex(ID3D12GraphicsCommandList4 *target, Context &) const, SetupTV(ID3D12GraphicsCommandList4 *target, Context &ctx) const;
#endif
};

inline Impl::Object3D::Subobject::Subobject(const AABB<3> &aabb, unsigned long int vcount, unsigned long int triOffset, unsigned short int tricount, ID3D12PipelineState *PSO,
	const float3 &albedo) :
	PSO(PSO), aabb(aabb), vcount(vcount), triOffset(triOffset), tricount(tricount),
	albedo(albedo),
#if INTEL_WORKAROUND
	FillMaterialSelector(&Subobject::FillAlbedoMaterial)
#else
	SetupSelector(&Subobject::SetupAlbedo)
#endif
{
}

inline Impl::Object3D::Subobject::Subobject(const AABB<3> &aabb, unsigned long int vcount, unsigned long int triOffset, unsigned short int tricount, ID3D12PipelineState *PSO,
	unsigned short int textureDescriptorTableOffset, bool tiled) :
	PSO(PSO), aabb(aabb), vcount(vcount), triOffset(triOffset), tricount(tricount),
	textureDescriptorTableOffset(textureDescriptorTableOffset), tiled(tiled),
#if INTEL_WORKAROUND
	FillMaterialSelector(&Subobject::FillTexMaterial)
#else
	SetupSelector(&Subobject::SetupTex)
#endif
{
}

inline Impl::Object3D::Subobject::Subobject(const AABB<3> &aabb, unsigned long int vcount, unsigned long int triOffset, unsigned short int tricount, ID3D12PipelineState *PSO,
	const float3 &albedo, float TVBrighntess, unsigned short int textureDescriptorTableOffset) :
	PSO(PSO), aabb(aabb), vcount(vcount), triOffset(triOffset), tricount(tricount),
	textureDescriptorTableOffset(textureDescriptorTableOffset), albedo(albedo), TVBrighntess(TVBrighntess),
#if INTEL_WORKAROUND
	FillMaterialSelector(&Subobject::FillTVMaterial)
#else
	SetupSelector(&Subobject::SetupTV)
#endif
{
}

#if INTEL_WORKAROUND
inline void Impl::Object3D::Subobject::FillMaterialCB(volatile MaterialData *dst) const
{
	(this->*FillMaterialSelector)(dst);
}
#endif

inline void Impl::Object3D::Subobject::Setup(ID3D12GraphicsCommandList4 *target, Context &ctx) const
{
	if (ctx.curPSO != PSO)
		target->SetPipelineState(ctx.curPSO = PSO);
#if !INTEL_WORKAROUND
	(this->*SetupSelector)(target, ctx);
#endif
}

inline auto Impl::Object3D::Subobject::SamplerDescriptorTableOffset() const noexcept
{
	return tiled * Descriptors::TextureSamplers::OBJ3D_COMMON_SAMPLERS_COUNT;
}

#if INTEL_WORKAROUND
inline void Impl::Object3D::Subobject::FillAlbedoMaterial(volatile MaterialData *dst) const
{
	const_cast<float3 &>(dst->albedo) = albedo;
}

inline void Impl::Object3D::Subobject::FillTexMaterial(volatile MaterialData *dst) const
{
	dst->textureDescriptorTableOffset = textureDescriptorTableOffset;
	dst->samplerDescriptorTableOffset = SamplerDescriptorTableOffset();
}

void Impl::Object3D::Subobject::FillTVMaterial(volatile MaterialData *dst) const
{
	FillAlbedoMaterial(dst);
	dst->textureDescriptorTableOffset = textureDescriptorTableOffset;
	dst->TVBrighntess = TVBrighntess;
}
#else
inline void Impl::Object3D::Subobject::SetupAlbedo(ID3D12GraphicsCommandList4 *cmdList, Context &ctx) const
{
	if (any(ctx.curAlbedo != albedo))
		cmdList->SetGraphicsRoot32BitConstants(ROOT_PARAM_MATERIAL, decltype(ctx.curAlbedo)::dimension, &(ctx.curAlbedo = albedo), 0);
}

inline void Impl::Object3D::Subobject::SetupTex(ID3D12GraphicsCommandList4 *cmdList, Context &) const
{
	cmdList->SetGraphicsRoot32BitConstant(ROOT_PARAM_MATERIAL, textureDescriptorTableOffset, decltype(Context::curAlbedo)::dimension);
}

void Impl::Object3D::Subobject::SetupTV(ID3D12GraphicsCommandList4 *cmdList, Context &ctx) const
{
	SetupAlbedo(cmdList, ctx);
	cmdList->SetGraphicsRoot32BitConstant(ROOT_PARAM_MATERIAL, textureDescriptorTableOffset | SamplerDescriptorTableOffset() << 16, decltype(Context::curAlbedo)::dimension);
	if (ctx.curTVBrighntess != TVBrighntess)
		cmdList->SetGraphicsRoot32BitConstant(ROOT_PARAM_MATERIAL, reinterpret_cast<const UINT &>(ctx.curTVBrighntess = TVBrighntess), decltype(Context::curAlbedo)::dimension + 1);	// strict aliasing rule violation, use C++20 bit_cast instead
}
#endif
#pragma endregion

#pragma region DescriptorTablePack
class Impl::Object3D::DescriptorTablePack final : Descriptors::GPUDescriptorHeap::AllocationClient
{
	vector<TrackedResource<ID3D12Resource>> textures;	// hold refs
	const shared_future<ComPtr<ID3D12DescriptorHeap>> CPUStore;

public:
#ifdef _MSC_VER
	DescriptorTablePack(vector<TrackedResource<ID3D12Resource>> &&textures, const wstring &objectName);
#else
	DescriptorTablePack(vector<TrackedResource<ID3D12Resource>> &&textures, const string &objectName);
#endif

private:
#ifdef _MSC_VER
	inline ComPtr<ID3D12DescriptorHeap> CreateBackingStore(const wstring &objectName);
#else
	inline ComPtr<ID3D12DescriptorHeap> CreateBackingStore(const string &objectName);
#endif

public:
	inline void Set(ID3D12GraphicsCommandList4 *target) const;

private:
	// Inherited via AllocationClient
	virtual void Commit(D3D12_CPU_DESCRIPTOR_HANDLE dst) const override;
};

#ifdef _MSC_VER
Impl::Object3D::DescriptorTablePack::DescriptorTablePack(vector<TrackedResource<ID3D12Resource>> &&textures, const wstring &objectName) :
#else
Impl::Object3D::DescriptorTablePack::DescriptorTablePack(vector<TrackedResource<ID3D12Resource>> &&textures, const string &objectName) :
#endif
	AllocationClient(textures.size()), textures(move(textures)), CPUStore(async(&DescriptorTablePack::CreateBackingStore, this, objectName))
{
}

#ifdef _MSC_VER
ComPtr<ID3D12DescriptorHeap> Impl::Object3D::DescriptorTablePack::CreateBackingStore(const wstring &objectName)
#else
ComPtr<ID3D12DescriptorHeap> Impl::Object3D::DescriptorTablePack::CreateBackingStore(const string &objectName)
#endif
{
	textures.shrink_to_fit();
	ComPtr<ID3D12DescriptorHeap> CPUStore;
	const D3D12_DESCRIPTOR_HEAP_DESC packDesc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	// type
		textures.size(),						// count
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE			// GPU invisible
	};
	CheckHR(device->CreateDescriptorHeap(&packDesc, IID_PPV_ARGS(CPUStore.GetAddressOf())));
#ifdef _MSC_VER
	NameObjectF(CPUStore.Get(), L"\"%ls\" descriptor table CPU backing store", objectName.c_str());
#else
	NameObjectF(CPUStore.Get(), L"\"%s\" descriptor table CPU backing store", objectName.c_str());
#endif
	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(packDesc.Type);
	// TODO: use C++20 initializer in range-based for
	CD3DX12_CPU_DESCRIPTOR_HANDLE dstDesc(CPUStore->GetCPUDescriptorHandleForHeapStart());
	for (const auto &srcTex : textures)
	{
		device->CreateShaderResourceView(srcTex.Get(), NULL, dstDesc);
		dstDesc.Offset(descriptorSize);
	}
	return CPUStore;
}

inline void Impl::Object3D::DescriptorTablePack::Set(ID3D12GraphicsCommandList4 *cmdList) const
{
	cmdList->SetGraphicsRootDescriptorTable(ROOT_PARAM_TEXTURE_DESC_TABLE, { GetGPUDescriptorsAllocation() });
}

void Impl::Object3D::DescriptorTablePack::Commit(D3D12_CPU_DESCRIPTOR_HANDLE dst) const
{
	const auto &backingStore = CPUStore.get();
	const auto packDesc = backingStore->GetDesc();
	device->CopyDescriptorsSimple(packDesc.NumDescriptors, dst, backingStore->GetCPUDescriptorHandleForHeapStart(), packDesc.Type);
}
#pragma endregion

WRL::ComPtr<ID3D12RootSignature> Impl::Object3D::CreateRootSig()
{
	namespace TextureSamplers = Descriptors::TextureSamplers;
	ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name);
	CD3DX12_ROOT_PARAMETER1 rootParams[ROOT_PARAM_COUNT];
	rootParams[ROOT_PARAM_PER_FRAME_DATA_CBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);									// per-frame data
	rootParams[ROOT_PARAM_TONEMAP_PARAMS_CBV].InitAsConstantBufferView(1, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);			// tonemap params
	rootParams[ROOT_PARAM_INSTANCE_DATA_CBV].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);	// instance data
#if INTEL_WORKAROUND
	rootParams[ROOT_PARAM_MATERIAL].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_PIXEL);
#else
	rootParams[ROOT_PARAM_MATERIAL].InitAsConstants(decltype(Context::curAlbedo)::dimension + 1, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
#endif
	// an unbounded range declared as STATIC means the rest of the heap is STATIC => specify VOLATILE
	const CD3DX12_DESCRIPTOR_RANGE1 descTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX/*unbounded*/, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	rootParams[ROOT_PARAM_TEXTURE_DESC_TABLE].InitAsDescriptorTable(1, &descTable, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[ROOT_PARAM_SAMPLER_DESC_TABLE] = TextureSamplers::GetDescTable(TextureSamplers::OBJECT3D_DESC_TABLE_ID, D3D12_SHADER_VISIBILITY_PIXEL);
	const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return CreateRootSignature(sigDesc, L"object 3D root signature");
}

auto Impl::Object3D::CreatePSOs() -> decltype(PSOs)
{
	decltype(PSOs) result;

	const CD3DX12_RASTERIZER_DESC rasterDesc
	(
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_BACK,
		TRUE,										// front CCW
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
		TRUE,																									// depth
		D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC_LESS,
		TRUE,																									// stencil
		D3D12_DEFAULT_STENCIL_READ_MASK,																		// stencil read mask
		D3D12_DEFAULT_STENCIL_WRITE_MASK,																		// stencil write mask
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_ALWAYS,		// front
		D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_COMPARISON_FUNC_ALWAYS		// back
	);

	// !: try to use less precision for normals (fixed point snorm)
	const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,	1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,	2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENTS",	0, DXGI_FORMAT_R32G32B32_FLOAT,	3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENTS",	1, DXGI_FORMAT_R32G32B32_FLOAT,	3, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	enum
	{
		VBDECLSIZE_FLAT	= 2,
		VBDECLSIZE_TEX	= 3,
		VBDECLSIZE_FULL = size(VB_decl),
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
	{
		.pRootSignature			= rootSig.Get(),
		.VS						= ShaderBytecode(Shaders::object3D_VS),
		.PS						= ShaderBytecode(Shaders::object3DFlat_PS),
		.BlendState				= CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask				= UINT_MAX,
		.RasterizerState		= rasterDesc,
		.DepthStencilState		= dsDesc,
		.InputLayout			= { VB_decl, VBDECLSIZE_FLAT },
		.IBStripCutValue		= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets		= 1,
		.RTVFormats				= { Config::HDRFormat },
		.DSVFormat				= Config::ZFormat,
		.SampleDesc				= Config::MSAA(),
		.Flags					= D3D12_PIPELINE_STATE_FLAG_NONE
	};

	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[false].flat.GetAddressOf())));
	NameObject(result[false].flat.Get(), L"object 3D [flat] PSO");

	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[true].flat.GetAddressOf())));
	NameObject(result[true].flat.Get(), L"object 3D [doublesided][flat] PSO");

	PSO_desc.InputLayout.NumElements = VBDECLSIZE_TEX;
	PSO_desc.VS = ShaderBytecode(Shaders::object3D_VS_UV);
	PSO_desc.PS = ShaderBytecode(Shaders::object3DTex_PS);
	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[false].tex[false].GetAddressOf())));
	NameObject(result[false].tex[false].Get(), L"object 3D [textured] PSO");

	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[true].tex[false].GetAddressOf())));
	NameObject(result[true].tex[false].Get(), L"object 3D [doublesided][textured] PSO");

	PSO_desc.PS = ShaderBytecode(Shaders::object3DAlphatest_PS);
	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[false].tex[true].GetAddressOf())));
	NameObject(result[false].tex[true].Get(), L"object 3D [alphatest] PSO");

	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[true].tex[true].GetAddressOf())));
	NameObject(result[true].tex[true].Get(), L"object 3D [doublesided][alphatest] PSO");

	PSO_desc.PS = ShaderBytecode(Shaders::object3DTV_PS);
	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[false].TV.GetAddressOf())));
	NameObject(result[false].TV.Get(), L"object 3D [TV] PSO");

	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[true].TV.GetAddressOf())));
	NameObject(result[true].TV.Get(), L"object 3D [doublesided][TV] PSO");

	PSO_desc.PS = ShaderBytecode(Shaders::object3DGlass_PS);
	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[false].advanced[GLASS_MASK_FLAG - 1].GetAddressOf())));
	NameObject(result[false].advanced[GLASS_MASK_FLAG - 1].Get(), L"object 3D [glass mask] PSO");

	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[true].advanced[GLASS_MASK_FLAG - 1].GetAddressOf())));
	NameObject(result[true].advanced[GLASS_MASK_FLAG - 1].Get(), L"object 3D [doublesided][glass mask] PSO");

	PSO_desc.InputLayout.NumElements = VBDECLSIZE_FULL;
	PSO_desc.VS = ShaderBytecode(Shaders::object3D_VS_UV_TG);
	PSO_desc.PS = ShaderBytecode(Shaders::object3DBump_PS);
	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[false].advanced[NORMAL_MAP_FLAG - 1].GetAddressOf())));
	NameObject(result[false].advanced[NORMAL_MAP_FLAG - 1].Get(), L"object 3D [normal map] PSO");

	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[true].advanced[NORMAL_MAP_FLAG - 1].GetAddressOf())));
	NameObject(result[true].advanced[NORMAL_MAP_FLAG - 1].Get(), L"object 3D [doublesided][normal map] PSO");

	PSO_desc.PS = ShaderBytecode(Shaders::object3DBumpGlass_PS);
	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[false].advanced[(NORMAL_MAP_FLAG | GLASS_MASK_FLAG) - 1].GetAddressOf())));
	NameObject(result[false].advanced[(NORMAL_MAP_FLAG | GLASS_MASK_FLAG) - 1].Get(), L"object 3D [normal map][glass mask] PSO");

	PSO_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(result[true].advanced[(NORMAL_MAP_FLAG | GLASS_MASK_FLAG) - 1].GetAddressOf())));
	NameObject(result[true].advanced[(NORMAL_MAP_FLAG | GLASS_MASK_FLAG) - 1].Get(), L"object 3D [doublesided][normal map][glass mask] PSO");

	return move(result);
}

namespace
{
	inline const auto &ExtractBase(const Object3D::SubobjectDataCallback::result_type &subobjData)
	{
		return visit([](const auto &src) noexcept -> const Object3D::SubobjectDataBase & { return src; }, subobjData);
	}

	const class SeqIterator final : public iterator<random_access_iterator_tag, unsigned short int, signed int>
	{
		value_type idx;

	public:
		explicit SeqIterator(value_type idx) noexcept : idx(idx) {}

	public:
		SeqIterator &operator ++() noexcept { return ++idx, *this; }
		SeqIterator operator ++(int) noexcept { return SeqIterator{ idx++ }; }
		SeqIterator operator +(difference_type offset) const noexcept { return SeqIterator(idx + offset); }
		friend SeqIterator operator -(SeqIterator a, SeqIterator b) noexcept { return SeqIterator(a.idx - b.idx); }

	public:
		value_type operator *() const noexcept { return idx; }
		friend auto operator <=>(SeqIterator a, SeqIterator b) noexcept { return a.idx <=> b.idx; }
	};
}

Impl::Object3D::Object3D(unsigned short int subobjCount, const SubobjectDataCallback &getSubobjectData, string name) :
	// use C++20 make_shared for arrays
	subobjects(new Subobject[subobjCount]), tricount(), subobjCount(subobjCount)
{
	if (!subobjCount)
		throw logic_error("Attempt to create empty 3D object");

	unsigned long int vcount = 0, uvcount = 0, tgcount = 0;

	enum
	{
		MAIN_TEXTURE,	// albedo map/TV emissive
		NORMAL_MAP,
		GLASS_MASK,
		TEXTURE_COUNT
	};
	vector<TrackedResource<ID3D12Resource>> texs;
	texs.reserve(subobjCount * TEXTURE_COUNT);

	// first pass
	for (unsigned short i = 0; i < subobjCount; i++)
	{
		const auto curSubobjData = getSubobjectData(i);
		const auto &curSubobjDataBase = ExtractBase(curSubobjData);
		const auto commonArgs = make_tuple(cref(curSubobjDataBase.aabb), curSubobjDataBase.vcount, tricount, curSubobjDataBase.tricount);

		const class SubobjParser final
		{
			decltype(commonArgs) &commonArgs;
			decltype(texs) &texs;
			unsigned long int &uvcount, &tgcount;

		public:
			constexpr SubobjParser(decltype(commonArgs) &commonArgs, decltype(texs) &texs, unsigned long int &uvcount, unsigned long int &tgcount) noexcept :
				commonArgs(commonArgs), texs(texs), uvcount(uvcount), tgcount(tgcount)
			{}

		public:
			Subobject operator ()(const SubobjectData<SubobjectType::Flat> &subobjFlat) const
			{
				ID3D12PipelineState *const PSO = PSOs[subobjFlat.doublesided].flat.Get();
				return make_from_tuple<Subobject>(tuple_cat(commonArgs, forward_as_tuple(PSO, subobjFlat.albedo)));
			}

			Subobject operator ()(const SubobjectData<SubobjectType::Tex> &subobjTex) const
			{
				ID3D12PipelineState *const PSO = PSOs[subobjTex.doublesided].tex[subobjTex.alphatest].Get();
				const unsigned int textureDescriptorTableOffset = texs.size();

				if (subobjTex.albedoMap.Usage() != TextureUsage::AlbedoMap)
					throw invalid_argument("3D object material: incompatible texture usage, albedo map expected.");
				texs.push_back(subobjTex.albedoMap.Acquire());
				uvcount += subobjTex.vcount;

				return make_from_tuple<Subobject>(tuple_cat(commonArgs, forward_as_tuple(PSO, textureDescriptorTableOffset, subobjTex.tiled)));
			}

			Subobject operator ()(const SubobjectData<SubobjectType::TV> &subobjTV) const
			{
				ID3D12PipelineState *const PSO = PSOs[subobjTV.doublesided].TV.Get();
				const unsigned int textureDescriptorTableOffset = texs.size();

				if (subobjTV.screen.Usage() != TextureUsage::TVScreen)
					throw invalid_argument("3D object material: incompatible texture usage, TV screen expected.");
				texs.push_back(subobjTV.screen.Acquire());
				uvcount += subobjTV.vcount;

				return make_from_tuple<Subobject>(tuple_cat(commonArgs, forward_as_tuple(PSO, subobjTV.albedo, subobjTV.brighntess, textureDescriptorTableOffset)));
			}

			Subobject operator ()(const SubobjectData<SubobjectType::Advanced> &subobjAdvanced) const
			{
				const auto materialFlags = -(bool)subobjAdvanced.normalMap & NORMAL_MAP_FLAG | -(bool)subobjAdvanced.glassMask & GLASS_MASK_FLAG;
				if (!materialFlags)
					throw logic_error("Advanced 3D object material provided without either normal map or glass mask");
				ID3D12PipelineState *const PSO = PSOs[subobjAdvanced.doublesided].advanced[materialFlags - 1].Get();
				const unsigned int textureDescriptorTableOffset = texs.size();

				// !: order of texture insertions is essential - it must match shader signature
				if (subobjAdvanced.albedoMap.Usage() != TextureUsage::AlbedoMap)
					throw invalid_argument("3D object material: incompatible texture usage, albedo map expected.");
				texs.push_back(subobjAdvanced.albedoMap.Acquire());
				uvcount += subobjAdvanced.vcount;
				if (subobjAdvanced.normalMap)
				{
					if (subobjAdvanced.normalMap.Usage() != TextureUsage::NormalMap)
						throw invalid_argument("3D object material: incompatible texture usage, normal map expected.");
					texs.push_back(subobjAdvanced.normalMap.Acquire());
					tgcount += subobjAdvanced.vcount;
				}
				if (subobjAdvanced.glassMask)
				{
					if (subobjAdvanced.glassMask.Usage() != TextureUsage::GlassMask)
						throw invalid_argument("3D object material: incompatible texture usage, glass mask expected.");
					texs.push_back(subobjAdvanced.glassMask.Acquire());
				}

				return make_from_tuple<Subobject>(tuple_cat(commonArgs, forward_as_tuple(PSO, textureDescriptorTableOffset, subobjAdvanced.tiled)));
			}
		} subobjParser(commonArgs, texs, uvcount, tgcount);

		vcount += curSubobjDataBase.vcount;
		tricount += curSubobjDataBase.tricount;
		subobjects[i] = visit(subobjParser, curSubobjData);
	}

#ifdef _MSC_VER
	// same workaround as for terrain quad
#if 0
	wstring_convert<codecvt_utf8<WCHAR>> converter;
	wstring convertedName = converter.from_bytes(name);
#else
	wstring convertedName(name.cbegin(), name.cend());
#endif
#endif

	// allocate descriptor table if needed
	if (!texs.empty())
	{
		if (texs.size() > USHRT_MAX)
			throw out_of_range("3D object material: too many textures.");
#ifdef _MSC_VER
		descriptorTablePack = make_shared<DescriptorTablePack>(move(texs), convertedName);
#else
		descriptorTablePack = make_shared<DescriptorTablePack>(move(texs), name);
#endif
	}

	// rearrange subobjects VBs so that fat ones (with uv and tangents) comes first
	{
		// alternative is to use itoa() with in-place sort
		const SeqIterator a{ 0 }, b{ subobjCount };

		const auto VBOrdering = [&getSubobjectData](unsigned short int left, unsigned short int right)
		{
			const auto VBComplexity = [&getSubobjectData](unsigned short int i)
			{
				// use C++20 templated lambda
				return visit([](const auto &subobjectData)
					{
						typedef remove_cvref_t<decltype(subobjectData)> SrcData;
						unsigned complexity = 0;
						if constexpr (is_base_of_v<SubobjectDataUV, SrcData>)
							complexity++;
						if constexpr (is_same_v<SrcData, SubobjectData<SubobjectType::Advanced>>)
							complexity += (bool)subobjectData.normalMap;
						return complexity;
					}, getSubobjectData(i));
			};

			return VBComplexity(left) > VBComplexity(right);
		};

		const auto FillVertexOffsets = [subobjCount, this](const auto &remapper)
		{
			unsigned long int vOffsetReordered = 0;
			for (unsigned int i = 0; i < subobjCount; i++)
			{
				auto &remapped = subobjects[remapper(i)];
				const auto vcount = remapped.vcount;
				remapped.vOffset = vOffsetReordered;
				vOffsetReordered += vcount;
			}
		};

		if (is_sorted(a, b, VBOrdering))	// potentially saves allocation
			FillVertexOffsets([](unsigned int i) noexcept { return i; });
		else
		{
			// use C++20 make_unique_default_init
			const auto remap = make_unique<unsigned short int []>(subobjCount);
			partial_sort_copy(a, b, remap.get(), remap.get() + subobjCount, VBOrdering);
			FillVertexOffsets([&remap](unsigned int i) { return remap[i]; });
		}
	}

	const unsigned long int
		VB_size = vcount * sizeof *SubobjectDataBase::verts,
		UVB_size = uvcount * sizeof *SubobjectDataUV::uv,
		TGB_size = tgcount * sizeof *SubobjectData<SubobjectType::Advanced>::tangents,
		IB_size = tricount * sizeof *SubobjectDataBase::tris;

	// create VIB
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(
#if INTEL_WORKAROUND
			sizeof(MaterialData) * subobjCount +
#endif
			VB_size * 2/*coord + N*/ + UVB_size + TGB_size + IB_size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,	// clear value
		IID_PPV_ARGS(VIB.GetAddressOf())));
#ifdef _MSC_VER
	NameObjectF(VIB.Get(), L"\"%ls\" geometry (contains %hu subobjects)", convertedName.c_str(), subobjCount);
#else
	NameObjectF(VIB.Get(), L"\"%s\" geometry (contains %hu subobjects)", name.c_str(), subobjCount);
#endif

	// fill VIB (second pass)
	{
#if INTEL_WORKAROUND
		volatile MaterialData *matPtr;
		CheckHR(VIB->Map(0, &CD3DX12_RANGE(0, 0), const_cast<void **>(reinterpret_cast<volatile void **>(&matPtr))));
		float (*const VB_ptr)[3] = reinterpret_cast<float (*)[3]>(const_cast<MaterialData *>(matPtr + subobjCount));
#else
		float (*VB_ptr)[3];
		CheckHR(VIB->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void **>(&VB_ptr)));
#endif
		float (*const NB_ptr)[3] = VB_ptr + vcount, (*const UVB_ptr)[2] = reinterpret_cast<float (*)[2]>(NB_ptr + vcount), (*const TGB_ptr)[2][3] = reinterpret_cast<float (*)[2][3]>(UVB_ptr + uvcount);
		uint16_t (*IB_ptr)[3] = reinterpret_cast<uint16_t (*)[3]>(TGB_ptr + tgcount);

		for (unsigned short i = 0; i < subobjCount; i++)
		{
			const auto curSubobjData = getSubobjectData(i);
			const auto &curSubobjDataBase = ExtractBase(curSubobjData);
			const auto &curSubobj = subobjects[i];

#if 0
			// calc AABB if it is not provided
			if (any(subobjects[i].aabb.Size() < 0.f))
				for (unsigned idx = 0; idx < curSubobjData.vcount; idx++)
					subobjects[i].aabb.Refit(curSubobjData.verts[idx]);
#endif

#if INTEL_WORKAROUND
			curSubobj.FillMaterialCB(matPtr++);
#endif
			memcpy(VB_ptr + curSubobj.vOffset, curSubobjDataBase.verts, curSubobjDataBase.vcount * sizeof *curSubobjDataBase.verts);
			memcpy(NB_ptr + curSubobj.vOffset, curSubobjDataBase.normals, curSubobjDataBase.vcount * sizeof *curSubobjDataBase.normals);
			// use C++20 templated lambda
			visit([UVB_ptr, TGB_ptr, &curSubobj](const auto &decodedSubobjData)
				{
					typedef remove_cvref_t<decltype(decodedSubobjData)> DecodedSubobjData;
					if constexpr (is_base_of_v<SubobjectDataUV, DecodedSubobjData>)
						memcpy(UVB_ptr + curSubobj.vOffset, decodedSubobjData.uv, decodedSubobjData.vcount * sizeof *decodedSubobjData.uv);
					if constexpr (is_same_v<DecodedSubobjData, SubobjectData<SubobjectType::Advanced>>)
						if (decodedSubobjData.normalMap)
						{
							if (!decodedSubobjData.tangents)
								throw logic_error("Advanced 3D object material provided with normal map but without tangents");
							memcpy(TGB_ptr + curSubobj.vOffset, decodedSubobjData.tangents, decodedSubobjData.vcount * sizeof *decodedSubobjData.tangents);
						}
				}, curSubobjData);
			memcpy(IB_ptr, curSubobjDataBase.tris, curSubobjDataBase.tricount * sizeof *curSubobjDataBase.tris);
			IB_ptr += curSubobjDataBase.tricount;
		}

		VIB->Unmap(0, NULL);
	}

	// start bundle creation
#ifdef _MSC_VER
	bundle = async(CreateBundle, subobjects, subobjCount, ComPtr<ID3D12Resource>(VIB), VB_size, UVB_size, TGB_size, IB_size, move(convertedName));
#else
	bundle = async(CreateBundle, subobjects, subobjCount, ComPtr<ID3D12Resource>(VIB), VB_size, UVB_size, TGB_size, IB_size, move(name));
#endif
}

Impl::Object3D::Object3D() = default;
Impl::Object3D::Object3D(const Object3D &) = default;
Impl::Object3D::Object3D(Object3D &&) = default;
Impl::Object3D &Impl::Object3D::operator =(const Object3D &) = default;
Impl::Object3D &Impl::Object3D::operator =(Object3D &&) = default;
Impl::Object3D::~Object3D() = default;

AABB<3> Impl::Object3D::GetXformedAABB(const float4x3 &xform) const
{
	AABB<3> result;

	/*
		transform every individual suboject AABB and then refit
		it somewhat slower than refitting in object space and transforming once for entire object but may provide tighter AABB
	*/
	for (unsigned i = 0; i < subobjCount; i++)
	{
		AABB<3> subobjAABB = subobjects[i].aabb;
		subobjAABB.Transform(xform);
		result.Refit(subobjAABB);
	}

	return result;
}

void Impl::Object3D::Setup(ID3D12GraphicsCommandList4 *cmdList, UINT64 frameDataGPUPtr, UINT64 tonemapParamsGPUPtr)
{
	using namespace Descriptors;
	cmdList->SetGraphicsRootSignature(rootSig.Get());
	cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_PER_FRAME_DATA_CBV, frameDataGPUPtr);
	cmdList->SetGraphicsRootConstantBufferView(ROOT_PARAM_TONEMAP_PARAMS_CBV, tonemapParamsGPUPtr);
	ID3D12DescriptorHeap *const descHeaps[] = { GPUDescriptorHeap::GetHeap().Get(), TextureSamplers::GetHeap().Get() };
	cmdList->SetDescriptorHeaps(size(descHeaps), descHeaps);
	cmdList->SetGraphicsRootDescriptorTable(ROOT_PARAM_SAMPLER_DESC_TABLE, TextureSamplers::GetGPUAddress(TextureSamplers::OBJECT3D_DESC_TABLE_ID));
}

ID3D12PipelineState *Impl::Object3D::GetStartPSO() const
{
	return subobjects[0].PSO;
}

const void Impl::Object3D::Render(ID3D12GraphicsCommandList4 *cmdList) const
{
	// bind descriptor table out of bundle so that descriptor heap changes would not cause bundle rebuild
	if (descriptorTablePack)
		descriptorTablePack->Set(cmdList);
	cmdList->ExecuteBundle(bundle.get().second.Get());
}

// need to copy subobjects to avoid dangling reference as the function can be executed in another thread
#ifdef _MSC_VER
auto Impl::Object3D::CreateBundle(const decltype(subobjects) &subobjects, unsigned short int subobjCount, ComPtr<ID3D12Resource> VIB,
	unsigned long int VB_size, unsigned long int UVB_size, unsigned long int TGB_size, unsigned long int IB_size, wstring &&objectName) -> decay_t<decltype(bundle.get())>
#else
auto Impl::Object3D::CreateBundle(const decltype(subobjects) &subobjects, unsigned short int subobjCount, ComPtr<ID3D12Resource> VIB,
	unsigned long int VB_size, unsigned long int UVB_size, unsigned long int TGB_size, unsigned long int IB_size, string &&objectName) -> decay_t<decltype(bundle.get())>
#endif
{
	decay_t<decltype(bundle.get())> bundle;	// to be returned

	// context
	Context ctx = { subobjects[0].PSO };

	// create command allocator
	CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(bundle.first.GetAddressOf())));
#ifdef _MSC_VER
	NameObjectF(bundle.first.Get(), L"\"%ls\" bundle allocator", objectName.c_str());
#else
	NameObjectF(bundle.first.Get(), L"\"%s\" bundle allocator", objectName.c_str());
#endif

	// create command list
	CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, bundle.first.Get(), ctx.curPSO, IID_PPV_ARGS(bundle.second.GetAddressOf())));
#ifdef _MSC_VER
	NameObjectF(bundle.second.Get(), L"\"%ls\" bundle", objectName.c_str());
#else
	NameObjectF(bundle.second.Get(), L"\"%s\" bundle", objectName.c_str());
#endif

	// record bundle
	{
		bundle.second->SetGraphicsRootSignature(rootSig.Get());
		bundle.second->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// setup VB/IB
		{
			const array<D3D12_VERTEX_BUFFER_VIEW, 4> VB_views =
			{
				{
					{
#if INTEL_WORKAROUND
						subobjCount * sizeof(MaterialData) +
#endif
						VIB->GetGPUVirtualAddress(),
						VB_size, sizeof *SubobjectDataBase::verts
					},
					{
						VB_views[0].BufferLocation + VB_views[0].SizeInBytes,
						VB_size, sizeof *SubobjectDataBase::normals
					},
					{
						VB_views[1].BufferLocation + VB_views[1].SizeInBytes,
						UVB_size, sizeof *SubobjectDataUV::uv
					},
					{
						VB_views[2].BufferLocation + VB_views[2].SizeInBytes,
						TGB_size, sizeof *SubobjectData<SubobjectType::Advanced>::tangents
					}
				}
			};
			const D3D12_INDEX_BUFFER_VIEW IB_view =
			{
				VB_views.back().BufferLocation + VB_views.back().SizeInBytes,
				IB_size,
				DXGI_FORMAT_R16_UINT
			};
			assert(UVB_size || !TGB_size);
			bundle.second->IASetVertexBuffers(0, 2 + bool(UVB_size) + bool(TGB_size)/*set UVB/TGB only if necessary*/, VB_views.data());
			bundle.second->IASetIndexBuffer(&IB_view);
		}

#if INTEL_WORKAROUND
		auto material_GPU_ptr = VIB->GetGPUVirtualAddress();
#endif
		for (unsigned i = 0; i < subobjCount; i++)
		{
			const auto &curSubobj = subobjects[i];

			curSubobj.Setup(bundle.second.Get(), ctx);
#if INTEL_WORKAROUND
			bundle.second->SetGraphicsRootConstantBufferView(ROOT_PARAM_MATERIAL, material_GPU_ptr), material_GPU_ptr += sizeof(MaterialData);
#endif
			bundle.second->DrawIndexedInstanced(curSubobj.tricount * 3, 1, curSubobj.triOffset * 3, curSubobj.vOffset, 0);
		}

		CheckHR(bundle.second->Close());
	}

	return move(bundle);
}