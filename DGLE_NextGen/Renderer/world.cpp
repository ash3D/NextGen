/**
\author		Alexey Shaydurov aka ASH
\date		12.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "world.hh"
#include "viewport.hh"
#include "terrain.hh"
#include "instance.hh"
#include "world hierarchy.inl"
#include "tracked resource.inl"
#include "cmdlist pool.inl"
#include "frustum culling.h"
#include "occlusion query shceduling.h"
#include "frame versioning.h"
#include "per-frame data.h"
#include "static objects data.h"
#include "GPU work submission.h"

using namespace std;
using namespace Renderer;
using namespace Math::VectorMath::HLSL;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

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
	NameObject(CB.Get(), L"per-frame CB");
	return move(CB);
}

auto Impl::World::MapPerFrameCB(const D3D12_RANGE *readRange) -> volatile PerFrameData *
{
	volatile PerFrameData *CPU_ptr;
	CheckHR(perFrameCB->Map(0, readRange, const_cast<void **>(reinterpret_cast<volatile void **>(&CPU_ptr))));
	return CPU_ptr;
}

// defined here, not in class in order to eliminate dependency on "instance.hh" in "world.hh"
#if defined _MSC_VER && _MSC_VER <= 1913
inline const AABB<3> &Impl::World::BVHObject::GetAABB() const noexcept
#else
inline const auto &Impl::World::BVHObject::GetAABB() const noexcept
#endif
{
	return instance->GetWorldAABB();
}

inline unsigned long int Impl::World::BVHObject::GetTriCount() const noexcept
{
	return instance->GetObject3D().GetTriCount();
}

void Impl::World::InstanceDeleter::operator()(const Renderer::Instance *instanceToRemove) const
{
	instanceToRemove->GetWorld()->InvalidateStaticObjects();
	instanceToRemove->GetWorld()->staticObjects.erase(instsnceLocation);
}

void Impl::World::InvalidateStaticObjects()
{
	staticObjectsCB.Reset();
	bvh.Reset();
}

//void Impl::World::MainPassPre(CmdListPool::CmdList &cmdList) const
//{
//}
//
//void Impl::World::MainPassPost(CmdListPool::CmdList &cmdList) const
//{
//}

void Impl::World::MainPassRange(unsigned long int rangeBegin, unsigned long int rangeEnd, CmdListPool::CmdList &cmdList) const
{
	assert(rangeBegin < rangeEnd);
	const auto objectsRangeBegin = next(staticObjects.cbegin(), rangeBegin), objectsRangeEnd = next(objectsRangeBegin, rangeEnd - rangeBegin);
	
	cmdList.Setup(objectsRangeBegin->GetStartPSO().Get());

	mainPassSetupCallback(cmdList);
	cmdList->SetGraphicsRootSignature(decltype(staticObjects)::value_type::GetRootSignature().Get());
	cmdList->SetGraphicsRootConstantBufferView(0, perFrameCB->GetGPUVirtualAddress() + PerFrameData::CurFrameCB_offset());

	using namespace placeholders;
	staticObjects.front().Render(cmdList);
#if defined _MSC_VER && _MSC_VER <= 1913
	for_each(objectsRangeBegin, objectsRangeEnd, bind(&Renderer::Instance::Render, _1, static_cast<ID3D12GraphicsCommandList1 *>(cmdList)));
#else
	for_each(objectsRangeBegin, objectsRangeEnd, bind(&decltype(staticObjects)::value_type::Render, _1, static_cast<ID3D12GraphicsCommandList1 *>(cmdList)));
#endif
}

//auto Impl::World::GetMainPassPre(unsigned int &length) const -> RenderPipeline::PipelineItem
//{
//	using namespace placeholders;
//	actionSelector = &World::GetMainPassRange;
//	return bind(&World::MainPassPre, this, _1);
//}

auto Impl::World::GetMainPassRange(unsigned int &length) const -> RenderPipeline::PipelineItem
{
	using namespace placeholders;
	return IterateRenderPass(length, staticObjects.size(), [] { RenderPipeline::TerminateStageTraverse();/*actionSelector = &World::GetMainPassPost;*/ },
		[this](unsigned long rangeBegin, unsigned long rangeEnd) { return bind(&World::MainPassRange, this, rangeBegin, rangeEnd, _1); });
}

//auto Impl::World::GetMainPassPost(unsigned int &length) const -> RenderPipeline::PipelineItem
//{
//	using namespace placeholders;
//	RenderPipeline::TerminateStageTraverse();
//	return bind(&World::MainPassPost, this, _1);
//}

Impl::World::World(const float (&terrainXform)[4][3])
{
	memcpy(this->terrainXform, terrainXform, sizeof terrainXform);
}

Impl::World::~World() = default;

template<unsigned int rows, unsigned int columns>
static inline void CopyMatrix2CB(const float (&src)[rows][columns], volatile Impl::CBRegister::AlignedRow<columns> (&dst)[rows]) noexcept
{
	copy_n(src, rows, dst);
}

void Impl::World::Render(const float (&viewXform)[4][3], const float (&projXform)[4][4], const function<void (bool enableRT, ID3D12GraphicsCommandList1 *target)> &setupRenderOutputCallback) const
{
	using namespace placeholders;

	FlushUpdates();

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
		auto &curFrameCB_region = perFrameCB_CPU_ptr[globalFrameVersioning->GetContinuousRingIdx()];
		CopyMatrix2CB(projXform, curFrameCB_region.projXform);
		CopyMatrix2CB(viewXform, curFrameCB_region.viewXform);
		CopyMatrix2CB(terrainXform, curFrameCB_region.terrainXform);
#if !PERSISTENT_MAPS
		range.End += sizeof(PerFrameData);
		perFrameCB->Unmap(0, &range);
#endif
	}

	const float4x3 terrainTransform(terrainXform);
	const float4x4 terrainFrustumXform = mul(float4x4(terrainTransform[0], 0.f, terrainTransform[1], 0.f, terrainTransform[2], 0.f, terrainTransform[3], 1.f), frustumTransform);
	const function<void (ID3D12GraphicsCommandList1 *target)> cullPassSetupCallback = bind(setupRenderOutputCallback, false, _1), mainPassSetupCallback = bind(setupRenderOutputCallback, true, _1);
	
	GPUWorkSubmission::AppendPipelineStage<true>(&World::BuildRenderStage, this, mainPassSetupCallback);

	for_each(terrainVectorLayers.cbegin(), terrainVectorLayers.cend(), bind(&decltype(terrainVectorLayers)::value_type::ShceduleRenderStage,
		_1, FrustumCuller<2>(terrainFrustumXform), cref(terrainFrustumXform), cref(cullPassSetupCallback), cref(mainPassSetupCallback)));
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

shared_ptr<Renderer::TerrainVectorLayer> Impl::World::AddTerrainVectorLayer(unsigned int layerIdx, const float (&color)[3], string layerName)
{
	// keep layers list sorted by idx
	class Idx
	{
		unsigned int idx;

	public:
		Idx(unsigned int idx) noexcept : idx(idx) {}
		Idx(const ::TerrainVectorLayer &layer) noexcept : idx(layer.layerIdx) {}

	public:
		operator unsigned int () const noexcept { return idx; }
	};
	const auto insertLocation = lower_bound(terrainVectorLayers.cbegin(), terrainVectorLayers.cend(), layerIdx, greater<Idx>());
	const auto inserted = terrainVectorLayers.emplace(insertLocation, shared_from_this(), layerIdx, color, move(layerName));
	// consider using custom allocator for shared_ptr's internal data in order to improve memory management
	return { &*inserted, [inserted](::TerrainVectorLayer *layerToRemove) { layerToRemove->world->terrainVectorLayers.erase(inserted); } };
}

auto Impl::World::AddStaticObject(Renderer::Object3D object, const float (&xform)[4][3], const AABB<3> &worldAABB) -> InstancePtr
{
	if (!object)
		throw logic_error("Attempt to add empty static object");
	InvalidateStaticObjects();
	const auto &inserted = staticObjects.emplace_back(shared_from_this(), move(object), xform, worldAABB);
	return { &inserted, { prev(staticObjects.cend()) } };
}

void Impl::World::FlushUpdates() const
{
	if (!staticObjects.empty())
	{
		struct AdressIterator : enable_if_t<true, decltype(staticObjects)::const_iterator>
		{
			AdressIterator(decltype(staticObjects)::const_iterator src) : enable_if_t<true, decltype(staticObjects)::const_iterator>(src) {}	// replace with C++17 aggregate base class init
			auto operator *() const noexcept { return &decltype(staticObjects)::const_iterator::operator *(); }
		};

		// rebuild BVH
		if (!bvh)
			bvh = { AdressIterator{staticObjects.cbegin()}, AdressIterator{staticObjects.cend()}, Hierarchy::SplitTechnique::MEAN, .5f };

		// recreate static objects CB
		if (!staticObjectsCB)
		{
			// create
			CheckHR(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(sizeof(StaticObjectData) * staticObjects.size()/*, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE*/),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				NULL,	// clear value
				IID_PPV_ARGS(staticObjectsCB.GetAddressOf())));
			NameObjectF(staticObjectsCB.Get(), L"static objects CB for world %p (%zu instances)", static_cast<const ::World *>(this), staticObjects.size());

			// fill
			volatile StaticObjectData *mapped;
			CheckHR(staticObjectsCB->Map(0, &CD3DX12_RANGE(0, 0), const_cast<void **>(reinterpret_cast<volatile void **>(&mapped))));
			// TODO: use C++20 initializer in range-based for
			auto CB_GPU_ptr = staticObjectsCB->GetGPUVirtualAddress();
			for (auto &instance : staticObjects)
			{
				CopyMatrix2CB(instance.GetWorldXform(), mapped++->worldform);
				instance.CB_GPU_ptr = CB_GPU_ptr;
				CB_GPU_ptr += sizeof(StaticObjectData);
			}
			staticObjectsCB->Unmap(0, NULL);
		}
	}
}

auto Impl::World::BuildRenderStage(std::function<void(ID3D12GraphicsCommandList1*target)> &mainPassSetupCallback) const -> RenderPipeline::RenderStage
{
	this->mainPassSetupCallback = move(mainPassSetupCallback);
	return { this, static_cast<decltype(actionSelector)>(&World::GetMainPassRange) };
}

/*
	VS 2017 STL uses allocator's construct() to construct combined object (main object + shared ptr data)
	but constructs containing object directly via placement new which does not have access to private members.
	GCC meanwhile compiles it fine.
*/
#if defined _MSC_VER && _MSC_VER <= 1913
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