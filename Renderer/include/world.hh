#pragma once

#define NOMINMAX

#include <cstddef>
#include <memory>
#include <string>
#include <list>
#include <future>
#include <wrl/client.h>
#include "../tracked resource.h"
#include "../AABB.h"
#include "../world hierarchy.h"
#include "../render pipeline.h"
#include "allocator adaptors.h"
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif

struct ID3D12Resource;
struct ID3D12GraphicsCommandList4;
struct D3D12_RANGE;

#if !_DEBUG
#define PERSISTENT_MAPS 1
#endif

extern void __cdecl InitRenderer();

namespace Renderer
{
	class Viewport;
	class TerrainVectorLayer;
	class Object3D;
	class Instance;
	class World;

	namespace TerrainMaterials
	{
		class Interface;
	}

	namespace Impl
	{
		namespace WRL = Microsoft::WRL;
		namespace HLSL = Math::VectorMath::HLSL;

		class Viewport;
		using Misc::AllocatorProxy;
		namespace RenderPipeline::RenderPasses
		{
			class PipelineROPTargets;
		}
		namespace RenderPasses = RenderPipeline::RenderPasses;

		class World : public std::enable_shared_from_this<Renderer::World>
		{
			friend extern void __cdecl ::InitRenderer();
			friend struct WorldViewContext;
			struct OcclusionQueryPasses;
			class MainRenderStage;
			class DebugRenderStage;
			typedef std::future<std::shared_ptr<const OcclusionQueryPasses>> StageExchange;

		private:
			// hazard tracking is not needed here - all the waiting required performed in globalFrameVersioning dtor
			static WRL::ComPtr<ID3D12Resource> globalGPUBuffer, CreateGlobalGPUBuffer();
			struct GlobalGPUBufferData;	// defined in "global GPU buffer data.h" to eliminate dependencies on d3d12.h here
			static volatile struct GlobalGPUBufferData
#if PERSISTENT_MAPS
				*globalGPUBuffer_CPU_ptr, *TryMapGlobalGPUBuffer(),
#endif
				*MapGlobalGPUBuffer(const D3D12_RANGE *readRange = NULL);

		protected:
			static UINT64 GetCurFrameGPUDataPtr();	// PerFrameData GPU address for cur frame

		private:
			// terrain
			float terrainXform[4][3];
			std::list<Renderer::TerrainVectorLayer, AllocatorProxy<Renderer::TerrainVectorLayer>> terrainVectorLayers;

		private:
			// sun
			struct
			{
				float zenith, azimuth;
			} sunDir;

		private:
			struct BVHObject
			{
				const Renderer::Instance *instance;

			public:
				BVHObject(const Renderer::Instance &instance) noexcept : instance(&instance) {}

			public:
				operator const Renderer::Instance *() const noexcept { return instance; }

			public:
#if defined _MSC_VER && _MSC_VER <= 1924
				inline const AABB<3> &GetAABB() const noexcept;
#else
				inline const auto &GetAABB() const noexcept;
#endif
				inline unsigned long int GetTriCount() const noexcept;
				float GetOcclusion() const noexcept { return .7f; }
			};

			//class NodeData
			//{
			//	WRL::ComPtr<ID3D12GraphicsCommandList4> bundle;
			//};

		private:
			// static objects
			mutable Hierarchy::BVH<Hierarchy::ENNEATREE, BVHObject> bvh;
			mutable decltype(bvh)::View bvhView;
			mutable std::list<Renderer::Instance, AllocatorProxy<Renderer::Instance>> staticObjects;
			mutable TrackedResource<ID3D12Resource> staticObjectsCB;
			struct StaticObjectData;
			void InvalidateStaticObjects();

		private:
			mutable size_t queryStreamLenCache{}, renderStreamsLenCache[2]{};

		private:
			class InstanceDeleter final
			{
				decltype(staticObjects)::const_iterator instanceLocation;

			public:
				InstanceDeleter() = default;
				explicit InstanceDeleter(decltype(instanceLocation) instanceLocation) : instanceLocation(instanceLocation) {}

			public:
				void operator ()(const Renderer::Instance *instanceToRemove) const;
			};

		protected:
			World(const float (&terrainXform)[4][3], float zenith, float azimuth);
			~World();
			World(World &) = delete;
			void operator =(World &) = delete;

		protected:
			void Render(struct WorldViewContext &viewCtx, const float (&viewXform)[4][3], const float (&projXform)[4][4], UINT64 tonemapParamsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets) const;
			static void OnFrameFinish();

		public:
			typedef std::unique_ptr<const Renderer::Instance, InstanceDeleter> InstancePtr;
			void SetSunDir(float zenith, float azimuth);
			std::shared_ptr<Renderer::Viewport> CreateViewport() const;
			std::shared_ptr<Renderer::TerrainVectorLayer> AddTerrainVectorLayer(std::shared_ptr<TerrainMaterials::Interface> layerMaterial, unsigned int layerIdx, std::string layerName);
			InstancePtr AddStaticObject(Renderer::Object3D object, const float (&xform)[4][3], const AABB<3> &worldAABB);
			void FlushUpdates() const;	// const to be able to call from Render()

		private:
			StageExchange ScheduleRenderStage(WorldViewContext &viewCtx, const HLSL::float4x4 &frustumTransform, const HLSL::float4x3 &worldViewTransform, UINT64 tonemapParamsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets) const;
			static void ScheduleDebugDrawRenderStage(UINT64 tonemapParamsGPUAddress, const RenderPasses::PipelineROPTargets &ROPTargets, StageExchange &&stageExchange);
		};
	}

	class World final : public Impl::World
	{
		template<class>
		friend class Misc::AllocatorProxyAdaptor;
		friend std::shared_ptr<World> __cdecl MakeWorld(const float (&terrainXform)[4][3], float zenith = 0, float azimuth = 0);
		friend class Impl::Viewport;
		friend class TerrainVectorQuad;	// for GetCurFrameGPUDataPtr()

	private:
		using Impl::World::World;
		~World() = default;
	};
}