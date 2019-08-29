#pragma once

#define NOMINMAX

#include <type_traits>
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <functional>
#include <future>
#include <wrl/client.h>
#include "../tracked resource.h"
#include "../AABB.h"
#include "../world hierarchy.h"
#include "../render pipeline.h"
#include "../GPU stream buffer allocator.h"
#include "allocator adaptors.h"
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12GraphicsCommandList4;
struct ID3D12Resource;

extern void __cdecl InitRenderer();

namespace Renderer
{
	namespace WRL = Microsoft::WRL;
	namespace HLSL = Math::VectorMath::HLSL;

	class World;

	namespace Impl
	{
		class World;
		class TerrainVectorLayer;

		template<unsigned int dimension>
		class FrustumCuller;

		using Misc::AllocatorProxy;
	}

	namespace TerrainMaterials
	{
		class Interface;
	}

	class TerrainVectorQuad final
	{
		template<class>
		friend class Misc::AllocatorProxyAdaptor;
		friend class Impl::TerrainVectorLayer;
		friend extern void __cdecl ::InitRenderer();
		struct OcclusionQueryPass;
		class MainRenderStage;
		class DebugRenderStage;
		typedef std::future<std::shared_ptr<const OcclusionQueryPass>> StageExchange;

	private:
		struct ObjectData
		{
			unsigned long int triCount;
			const void *tris;
			const AABB<2> &aabb;
		};

		struct Object
		{
			AABB<2> aabb;
			unsigned long int triCount;
			unsigned int idx;

			// interface for BVH
		public:
#if defined _MSC_VER && _MSC_VER <= 1922
			const AABB<2> &GetAABB() const noexcept { return aabb; }
#else
			const auto &GetAABB() const noexcept { return aabb; }
#endif
			unsigned long int GetTriCount() const noexcept { return triCount; }
			float GetOcclusion() const noexcept { return .7f; }
		};

		struct NodeCluster
		{
			unsigned long int startIdx;
		};

	private:
		const std::shared_ptr<class TerrainVectorLayer> layer;
		Impl::Hierarchy::BVH<Impl::Hierarchy::QUADTREE, Object, NodeCluster> subtree;
		mutable decltype(subtree)::View subtreeView;
		Impl::TrackedResource<ID3D12Resource> VIB;	// Vertex/Index Buffer
		const bool IB32bit;
		const unsigned long int VB_size, IB_size;

	private:
		typedef decltype(subtree)::Node TreeNode;
		typedef decltype(subtreeView)::Node ViewNode;

	private:
		TerrainVectorQuad(std::shared_ptr<class TerrainVectorLayer> &&layer, unsigned long int vcount, const std::function<void (volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const std::function<ObjectData (unsigned int objIdx)> &getObjectData);
		~TerrainVectorQuad();
		TerrainVectorQuad(TerrainVectorQuad &) = delete;
		void operator =(TerrainVectorQuad &) = delete;

	private:
		static constexpr const WCHAR AABB_VB_name[] = L"terrain occlusion query quads";
		void Schedule(Impl::GPUStreamBuffer::Allocator<sizeof AABB<2>, AABB_VB_name> &GPU_AABB_allocator, const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform) const;
		void Issue(MainRenderStage &renderStage, std::remove_const_t<decltype(Impl::OcclusionCulling::QueryBatchBase::npos)> &occlusionProvider) const;
	};

	namespace Impl
	{
		namespace RenderPipeline::RenderPasses
		{
			class PipelineROPTargets;
		}

		class TerrainVectorLayer : public std::enable_shared_from_this<Renderer::TerrainVectorLayer>
		{
			typedef TerrainVectorQuad::MainRenderStage MainRenderStage;
			typedef TerrainVectorQuad::DebugRenderStage DebugRenderStage;
			friend class MainRenderStage;
			friend class DebugRenderStage;

		protected:
			typedef TerrainVectorQuad::StageExchange StageExchange;

		protected:
			const std::shared_ptr<class Renderer::World> world;
			const unsigned int layerIdx;
			const std::string layerName;

		private:
			const std::shared_ptr<TerrainMaterials::Interface> layerMaterial;
			std::list<class TerrainVectorQuad, AllocatorProxy<class TerrainVectorQuad>> quads;

		private:
			mutable size_t queryStreamLenCache{}, renderStreamLenCache{};

		private:
			class QuadDeleter final
			{
				decltype(quads)::const_iterator quadLocation;

			public:
				QuadDeleter() = default;
				explicit QuadDeleter(decltype(quadLocation) quadLocation) : quadLocation(quadLocation) {}

			public:
				void operator ()(const class TerrainVectorQuad *quadToRemove) const;
			};

		protected:
			TerrainVectorLayer(std::shared_ptr<class Renderer::World> &&world, std::shared_ptr<TerrainMaterials::Interface> &&layerMaterial, unsigned int layerIdx, std::string &&layerName);
			~TerrainVectorLayer();
			TerrainVectorLayer(TerrainVectorLayer &) = delete;
			void operator =(TerrainVectorLayer &) = delete;

		public:
			typedef TerrainVectorQuad::ObjectData ObjectData;
			typedef std::unique_ptr<class TerrainVectorQuad, QuadDeleter> QuadPtr;
			QuadPtr AddQuad(unsigned long int vcount, const std::function<void __cdecl (volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const std::function<ObjectData __cdecl(unsigned int objIdx)> &getObjectData);

		protected:
			StageExchange ScheduleRenderStage(const FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform, UINT64 tonemapParamsGPUAddress, const RenderPipeline::RenderPasses::PipelineROPTargets &ROPTargets) const;
			static void ScheduleDebugDrawRenderStage(UINT64 tonemapParamsGPUAddress, const RenderPipeline::RenderPasses::PipelineROPTargets &ROPTargets, StageExchange &&stageExchange);
			static void OnFrameFinish();
		};
	}

	class TerrainVectorLayer final : public Impl::TerrainVectorLayer
	{
		template<class>
		friend class Misc::AllocatorProxyAdaptor;
		friend class TerrainVectorQuad;
		friend class Impl::World;

	private:
		using Impl::TerrainVectorLayer::TerrainVectorLayer;
		~TerrainVectorLayer() = default;
#if defined _MSC_VER && _MSC_VER <= 1922 && !defined __clang__
		// this workaround makes '&TerrainVectorLayer::ScheduleRenderStage' accessible from 'Impl::World'\
		somewhat strange as the problem does not reproduce for simple synthetic experiment
		using Impl::TerrainVectorLayer::ScheduleRenderStage;
		using Impl::TerrainVectorLayer::ScheduleDebugDrawRenderStage;
#endif
	};
}