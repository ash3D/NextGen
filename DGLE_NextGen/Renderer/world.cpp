/**
\author		Alexey Shaydurov aka ASH
\date		14.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "world.hh"
#include "viewport.hh"
#include "terrain.hh"
#include "world hierarchy.inl"
#include "frustum culling.h"
#include "occlusion query shceduling.h"
#include "frame versioning.h"

#include "AABB_2d.csh"
#include "vectorLayerVS.csh"
#include "vectorLayerPS.csh"

using namespace std;
using namespace Renderer;
using namespace Math::VectorMath::HLSL;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;

static constexpr unsigned int CBRegisterBits = D3D12_COMMONSHADER_CONSTANT_BUFFER_COMPONENTS * D3D12_COMMONSHADER_CONSTANT_BUFFER_COMPONENT_BIT_COUNT, CBRegisterAlign = CBRegisterBits / CHAR_BIT;
static_assert(CBRegisterBits % CHAR_BIT == 0);

template<unsigned int length>
struct alignas(CBRegisterAlign) CBRegisterAlignedRow : array<float, length>
{
	void operator =(const float (&src)[length]) volatile noexcept
	{
		memcpy(const_cast<CBRegisterAlignedRow *>(this)->data(), src, sizeof src);
	}
};

struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) Impl::World::PerFrameData
{
	CBRegisterAlignedRow<4> projXform[4];
	CBRegisterAlignedRow<3> viewXform[4], worldXform[4];
};

ComPtr<ID3D12Resource> Impl::World::CreatePerFrameCB()
{
	ComPtr<ID3D12Resource> CB;
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(PerFrameData) * Impl::maxFrameLatency/*, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE*/),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,	// clear value
		IID_PPV_ARGS(CB.GetAddressOf())));
	return move(CB);
}

auto Impl::World::MapPerFrameCB(const D3D12_RANGE *readRange) -> volatile PerFrameData *
{
	volatile PerFrameData *CPU_ptr;
	CheckHR(perFrameCB->Map(0, readRange, const_cast<void **>(reinterpret_cast<volatile void **>(&CPU_ptr))));
	return CPU_ptr;
}

static void CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, ComPtr<ID3D12RootSignature> &target)
{
	ComPtr<ID3DBlob> sig, error;
	const HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &sig, &error);
	if (error)
	{
		cerr.write((const char *)error->GetBufferPointer(), error->GetBufferSize()) << endl;
	}
	CheckHR(hr);
	CheckHR(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&target)));
}

Impl::World::World(const float(&terrainXform)[4][3])// : bvh(Hierarchy::SplitTechnique::MEAN, .5f)
{
	// create terrain cull pass root signature
	{
		CD3DX12_ROOT_PARAMETER1 CBV_param;
		CBV_param.InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(1, &CBV_param, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		CreateRootSignature(sigDesc, terrain.vectorLayerD3DObjs.cullPassRootSig);
	}

	// create terrain main pass root signature
	{
		CD3DX12_ROOT_PARAMETER1 rootParams[2];
		rootParams[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParams[1].InitAsConstants(3, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		const CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC sigDesc(size(rootParams), rootParams, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		CreateRootSignature(sigDesc, terrain.vectorLayerD3DObjs.mainPassRootSig);
	}

	// create terrain cull pass PSO
	{
		const D3D12_BLEND_DESC blendDesc
		{
			FALSE,						// alpha2covarage
			FALSE,						// independent blend
			{
				FALSE,					// blend enable
				FALSE,					// logic op enable
				D3D12_BLEND_ONE,		// src blend
				D3D12_BLEND_ZERO,		// dst blend
				D3D12_BLEND_OP_ADD,		// blend op
				D3D12_BLEND_ONE,		// src blend alpha
				D3D12_BLEND_ZERO,		// dst blend alpha
				D3D12_BLEND_OP_ADD,		// blend op alpha
				D3D12_LOGIC_OP_NOOP,	// logic op
				0						// write mask
			}
		};

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
			0,																										// stencil write mask
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_NOT_EQUAL,	// front
			D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_NOT_EQUAL	// back
		);

		const D3D12_INPUT_ELEMENT_DESC VB_decl[] =
		{
			{ "AABB_min", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "AABB_max", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC PSO_desc =
		{
			terrain.vectorLayerD3DObjs.cullPassRootSig.Get(),				// root signature
			CD3DX12_SHADER_BYTECODE(AABB_2D, sizeof AABB_2D),				// VS
			{},																// PS
			{},																// DS
			{},																// HS
			{},																// GS
			{},																// SO
			blendDesc,														// blend
			UINT_MAX,														// sample mask
			rasterDesc,														// rasterizer
			dsDesc,															// depth stencil
			{ VB_decl, size(VB_decl) },										// IA
			D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,					// restart primtive
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,							// primitive topology
			0,																// render targets
			{},																// RT formats
			DXGI_FORMAT_D24_UNORM_S8_UINT,									// depth stencil format
			{1}																// MSAA
		};

		CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(&terrain.vectorLayerD3DObjs.cullPassPSO)));
	}

	// create terrain main pass PSO
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
			terrain.vectorLayerD3DObjs.mainPassRootSig.Get(),				// root signature
			CD3DX12_SHADER_BYTECODE(vectorLayerVS, sizeof vectorLayerVS),	// VS
			CD3DX12_SHADER_BYTECODE(vectorLayerPS, sizeof vectorLayerPS),	// PS
			{},																// DS
			{},																// HS
			{},																// GS
			{},																// SO
			CD3DX12_BLEND_DESC(D3D12_DEFAULT),								// blend
			UINT_MAX,														// sample mask
			rasterDesc,														// rasterizer
			dsDesc,															// depth stencil
			{ VB_decl, size(VB_decl) },										// IA
			D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,					// restart primtive
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,							// primitive topology
			1,																// render targets
			{ DXGI_FORMAT_R8G8B8A8_UNORM },									// RT formats
			DXGI_FORMAT_D24_UNORM_S8_UINT,									// depth stencil format
			{1}																// MSAA
		};

		CheckHR(device->CreateGraphicsPipelineState(&PSO_desc, IID_PPV_ARGS(&terrain.vectorLayerD3DObjs.mainPassPSO)));
	}

	memcpy(terrain.xform, terrainXform, sizeof terrainXform);
}

Impl::World::~World() = default;

template<unsigned int rows, unsigned int columns>
static inline void CopyMatrix2CB(const float (&src)[rows][columns], volatile CBRegisterAlignedRow<columns> (&dst)[rows]) noexcept
{
	copy_n(src, rows, dst);
}

void Impl::World::Render(const float (&viewXform)[4][3], const float (&projXform)[4][4], const function<void (bool enableRT, ID3D12GraphicsCommandList1 *target)> &setupRenderOutputCallback) const
{
	using namespace placeholders;

	const float4x3 viewTransform(viewXform);
	const float4x4 frustumTransform = mul(float4x4(viewTransform[0], 0.f, viewTransform[1], 0.f, viewTransform[2], 0.f, viewTransform[3], 1.f), float4x4(projXform));
	//bvh.Shcedule(/*bind(&World::ScheduleNode, this, _1),*/ frustumTransform, &viewTransform);

	// update per-frame CB
	const auto curFrameCB_offset = sizeof(PerFrameData) * globalFrameVersioning->GetRingBufferIdx();
	{
#if !PERSISTENT_MAPS
		CD3DX12_RANGE range(curFrameCB_offset, curFrameCB_offset);
		volatile PerFrameData *const perFrameCB_CPU_ptr = MapPerFrameCB(&range);
#endif
		auto &curFrameCB_region = perFrameCB_CPU_ptr[globalFrameVersioning->GetRingBufferIdx()];
		CopyMatrix2CB(projXform, curFrameCB_region.projXform);
		CopyMatrix2CB(viewXform, curFrameCB_region.viewXform);
		CopyMatrix2CB(terrain.xform, curFrameCB_region.worldXform);
#if !PERSISTENT_MAPS
		range.End += sizeof(PerFrameData);
		perFrameCB->Unmap(0, &range);
#endif
	}

	const float4x3 terrainTransform(terrain.xform);
	const float4x4 terrainFrustumXform = mul(float4x4(terrainTransform[0], 0.f, terrainTransform[1], 0.f, terrainTransform[2], 0.f, terrainTransform[3], 1.f), frustumTransform);
	const function<void (ID3D12GraphicsCommandList1 *target)> cullPassSetupCallback =
		[
			&setupRenderOutputCallback,
			rootSig = terrain.vectorLayerD3DObjs.cullPassRootSig,
			CB_location = perFrameCB->GetGPUVirtualAddress() + curFrameCB_offset
		](ID3D12GraphicsCommandList1 *cmdList)
	{
		setupRenderOutputCallback(false, cmdList);
		cmdList->SetGraphicsRootSignature(rootSig.Get());
		cmdList->SetGraphicsRootConstantBufferView(0, CB_location);
	};
	const function<void (ID3D12GraphicsCommandList1 *target)> terrainMainPassSetupCallback =
		[
			&setupRenderOutputCallback,
			rootSig = terrain.vectorLayerD3DObjs.mainPassRootSig,
			CB_location = perFrameCB->GetGPUVirtualAddress() + curFrameCB_offset
		](ID3D12GraphicsCommandList1 *cmdList)
	{
		setupRenderOutputCallback(true, cmdList);
		cmdList->SetGraphicsRootSignature(rootSig.Get());
		cmdList->SetGraphicsRootConstantBufferView(0, CB_location);
	};
	for_each(terrain.vectorLayers.cbegin(), terrain.vectorLayers.cend(), bind(&decltype(terrain.vectorLayers)::value_type::ShceduleRenderStage,
		_1, FrustumCuller<2>(terrainFrustumXform), cref(terrainFrustumXform), cref(cullPassSetupCallback), cref(terrainMainPassSetupCallback)));
}

// "world.hh" currently does not #include "terrain.hh" (TerrainVectorLayer forward declared) => out-of-line
void Impl::World::OnFrameFinish() const
{
	TerrainVectorLayer::OnFrameFinish();
}

//void Impl::World::ScheduleNode(decltype(bvh)::Node &node) const
//{
//}

shared_ptr<Renderer::Viewport> Impl::World::CreateViewport() const
{
	return make_shared<Renderer::Viewport>(shared_from_this());
}

auto Impl::World::AddTerrainVectorLayer(unsigned int layerIdx, const float (&color)[3]) -> shared_ptr<TerrainVectorLayer>
{
	// keep layers list sorted by idx
	class Idx
	{
		unsigned int idx;

	public:
		Idx(unsigned int idx) noexcept : idx(idx) {}
		Idx(const TerrainVectorLayer &layer) noexcept : idx(layer.layerIdx) {}

	public:
		operator unsigned int () const noexcept { return idx; }
	};
	const auto insertLocation = lower_bound(terrain.vectorLayers.cbegin(), terrain.vectorLayers.cend(), layerIdx, greater<Idx>());
	const auto inserted = terrain.vectorLayers.emplace(insertLocation, shared_from_this(), layerIdx, color, terrain.vectorLayerD3DObjs.cullPassPSO, terrain.vectorLayerD3DObjs.mainPassPSO);
	// consider using custom allocator for shared_ptr's internal data in order to improve memory management
	return { &*inserted, [inserted](TerrainVectorLayer *layerToRemove) { layerToRemove->world->terrain.vectorLayers.erase(inserted); } };
}

/*
	VS 2017 STL uses allocator's construct() to construct combined object (main object + shared ptr data)
	but constructs containing object directly via placement new which does not have access to private members.
	GCC meanwhile compiles it fine.
*/
#if defined _MSC_VER && _MSC_VER <= 1912
shared_ptr<World> __cdecl Renderer::MakeWorld(const float (&terrainXform)[4][3])
{
	return make_shared<World>(World::tag(), terrainXform);
}
#else
shared_ptr<World> __cdecl Renderer::MakeWorld(const float (&terrainXform)[4][3])
{
	return allocate_shared<World>(World::Allocator<World>(), terrainXform);
}
#endif