/**
\author		Alexey Shaydurov aka ASH
\date		15.01.2018 (c)Korotkov Andrey

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
#include "per-frame data.h"

using namespace std;
using namespace Renderer;
using namespace Math::VectorMath::HLSL;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;

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

Impl::World::World(const float (&terrainXform)[4][3])// : bvh(Hierarchy::SplitTechnique::MEAN, .5f)
{
	memcpy(this->terrainXform, terrainXform, sizeof terrainXform);
}

Impl::World::~World() = default;

template<unsigned int rows, unsigned int columns>
static inline void CopyMatrix2CB(const float (&src)[rows][columns], volatile Impl::CBRegisterAlignedRow<columns> (&dst)[rows]) noexcept
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
	{
#if !PERSISTENT_MAPS
		const auto curFrameCB_offset = PerFrameData::CurFrameCB_offset();
		CD3DX12_RANGE range(curFrameCB_offset, curFrameCB_offset);
		volatile PerFrameData *const perFrameCB_CPU_ptr = MapPerFrameCB(&range);
#endif
		auto &curFrameCB_region = perFrameCB_CPU_ptr[globalFrameVersioning->GetRingBufferIdx()];
		CopyMatrix2CB(projXform, curFrameCB_region.projXform);
		CopyMatrix2CB(viewXform, curFrameCB_region.viewXform);
		CopyMatrix2CB(terrainXform, curFrameCB_region.worldXform);
#if !PERSISTENT_MAPS
		range.End += sizeof(PerFrameData);
		perFrameCB->Unmap(0, &range);
#endif
	}

	const float4x3 terrainTransform(terrainXform);
	const float4x4 terrainFrustumXform = mul(float4x4(terrainTransform[0], 0.f, terrainTransform[1], 0.f, terrainTransform[2], 0.f, terrainTransform[3], 1.f), frustumTransform);
	const function<void (ID3D12GraphicsCommandList1 *target)> cullPassSetupCallback = bind(setupRenderOutputCallback, false, _1), terrainMainPassSetupCallback = bind(setupRenderOutputCallback, true, _1);
	for_each(terrainVectorLayers.cbegin(), terrainVectorLayers.cend(), bind(&decltype(terrainVectorLayers)::value_type::ShceduleRenderStage,
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
	const auto insertLocation = lower_bound(terrainVectorLayers.cbegin(), terrainVectorLayers.cend(), layerIdx, greater<Idx>());
	const auto inserted = terrainVectorLayers.emplace(insertLocation, shared_from_this(), layerIdx, color);
	// consider using custom allocator for shared_ptr's internal data in order to improve memory management
	return { &*inserted, [inserted](TerrainVectorLayer *layerToRemove) { layerToRemove->world->terrainVectorLayers.erase(inserted); } };
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