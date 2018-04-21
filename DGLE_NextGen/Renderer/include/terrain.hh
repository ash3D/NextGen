/**
\author		Alexey Shaydurov aka ASH
\date		21.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#define NOMINMAX

#include <type_traits>
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <functional>
#include <optional>
#include <variant>
#include <wrl/client.h>
#include "world.hh"	// temp for Allocator
#include "../tracked resource.h"
#include "../AABB.h"
#include "../world hierarchy.h"
#include "../render stage.h"
#include "../render pipeline.h"
#include "../GPU stream buffer allocator.h"
#include "../occlusion query batch.h"
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12GraphicsCommandList2;
struct ID3D12Resource;

extern void __cdecl InitRenderer();

namespace Renderer
{
	namespace WRL = Microsoft::WRL;
	namespace HLSL = Math::VectorMath::HLSL;

	namespace Impl
	{
		class World;
		class TerrainVectorLayer;

		template<unsigned int dimension>
		class FrustumCuller;
	}

	class TerrainVectorQuad final
	{
		friend class Impl::TerrainVectorLayer;

		template<typename>
		friend class World::Allocator;

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
#if defined _MSC_VER && _MSC_VER <= 1913
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
		TerrainVectorQuad(std::shared_ptr<class TerrainVectorLayer> &&layer, unsigned long int vcount, const std::function<void (volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const std::function<ObjectData (unsigned int objIdx)> &getObjectData);
		~TerrainVectorQuad();
		TerrainVectorQuad(TerrainVectorQuad &) = delete;
		void operator =(TerrainVectorQuad &) = delete;

	private:
		static constexpr const WCHAR AABB_VB_name[] = L"terrain occlusion query quads";
		void Schedule(Impl::GPUStreamBuffer::Allocator<sizeof AABB<2>, AABB_VB_name> &GPU_AABB_allocator, const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform) const, Issue(std::remove_const_t<decltype(Impl::OcclusionCulling::QueryBatchBase::npos)> &occlusionProvider) const;
	};

	namespace Impl
	{
		class TerrainVectorLayer : public std::enable_shared_from_this<Renderer::TerrainVectorLayer>, RenderPipeline::IRenderStage
		{
			friend class TerrainVectorQuad;
			friend extern void __cdecl ::InitRenderer();

		protected:
			const std::shared_ptr<class Renderer::World> world;
			const unsigned int layerIdx;

		private:
			const float color[3];
			const std::string layerName;
			std::list<class TerrainVectorQuad, World::Allocator<class TerrainVectorQuad>> quads;

		private:
			typedef decltype(TerrainVectorQuad::subtree)::Node TreeNode;
			typedef decltype(TerrainVectorQuad::subtreeView)::Node ViewNode;

#pragma region occlusion query pass
		private:
			static WRL::ComPtr<ID3D12RootSignature> cullPassRootSig, TryCreateCullPassRootSig(), CreateCullPassRootSig();
			static WRL::ComPtr<ID3D12PipelineState> cullPassPSO, TryCreateCullPassPSO(), CreateCullPassPSO();

		private:
			mutable std::function<void (ID3D12GraphicsCommandList2 *target)> cullPassSetupCallback;
			struct OcclusionQueryGeometry
			{
				ID3D12Resource *VB;
				unsigned long int startIdx;
				unsigned int count;
			};
			mutable std::vector<OcclusionQueryGeometry> queryStream;

		private:
			inline void CullPassPre(ID3D12GraphicsCommandList2 *target) const, CullPassPost(ID3D12GraphicsCommandList2 *target) const;
			void CullPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd) const;

		private:
			void SetupCullPass(std::function<void (ID3D12GraphicsCommandList2 *target)> &&setupCallback) const;
			void IssueOcclusion(ViewNode::OcclusionQueryGeometry occlusionQueryGeometry);
#pragma endregion

#pragma region main pass
		private:
			static WRL::ComPtr<ID3D12RootSignature> mainPassRootSig, TryCreateMainPassRootSig(), CreateMainPassRootSig();
			static WRL::ComPtr<ID3D12PipelineState> mainPassPSO, TryCreateMainPassPSO(), CreateMainPassPSO();

		private:
			mutable std::function<void (ID3D12GraphicsCommandList2 *target)> mainPassSetupCallback;
			struct RenderData
			{
				unsigned long int startIdx, triCount;
				decltype(OcclusionCulling::QueryBatchBase::npos) occlusion;
				unsigned long int startQuadIdx;
			};
			struct Quad
			{
				unsigned long int streamEnd;
				ID3D12Resource *VIB;	// no reference/lifetime tracking required, it would induce unnecessary overhead (lifetime tracking leads to mutex locs)
				unsigned long int VB_size, IB_size;
				bool IB32bit;
			};
			mutable std::vector<RenderData> renderStream;
			mutable std::vector<Quad> quadStram;

		private:
			inline void MainPassPre(ID3D12GraphicsCommandList2 *target) const, MainPassPost(ID3D12GraphicsCommandList2 *target) const;
			void MainPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd) const;

		private:
			void SetupMainPass(std::function<void (ID3D12GraphicsCommandList2 *target)> &&setupCallback) const;
			void IssueCluster(unsigned long int startIdx, unsigned long int triCount, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion);
			void IssueExclusiveObjects(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion);
			void IssueChildren(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion);
			void IssueWholeNode(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion);
			bool IssueNodeObjects(const TreeNode &node, decltype(OcclusionCulling::QueryBatchBase::npos) coarseOcclusion,  decltype(OcclusionCulling::QueryBatchBase::npos) fineOcclusion, ViewNode::Visibility visibility);
			void IssueQuad(ID3D12Resource *VIB, unsigned long int VB_size, unsigned long int IB_size, bool IB32bit);
#pragma endregion

#pragma region visualize occlusion pass
		private:
			// reuse main pass root signature for now
			static WRL::ComPtr<ID3D12PipelineState> AABB_PSO, TryCreateAABB_PSO(), CreateAABB_PSO();

		private:
			void AABBPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, const float (&color)[3], bool visible) const;
#pragma endregion

		private:
			void StagePre(CmdListPool::CmdList &target) const, StagePost(CmdListPool::CmdList &target) const;
			void CullPass2MainPass(CmdListPool::CmdList &target) const;

		private:
			// order is essential (TRANSIENT, then PERSISTENT), index based access used
			mutable std::variant<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>, OcclusionCulling::QueryBatch<OcclusionCulling::PERSISTENT>> occlusionQueryBatch;

		private:
			// Inherited via IRenderStage
			virtual void Sync() const override final;

		private:
			RenderPipeline::PipelineItem
				GetStagePre(unsigned int &length) const, GetStagePost(unsigned int &length) const,
				GetCullPassRange(unsigned int &length) const, GetMainPassRange(unsigned int &length) const,
				GetCullPass2MainPass(unsigned int &length) const,
				GetVisiblePassRange(unsigned int &length) const, GetCulledPassRange(unsigned int &length) const;

		private:
			void Setup(std::function<void (ID3D12GraphicsCommandList2 *target)> &&cullPassSetupCallback, std::function<void (ID3D12GraphicsCommandList2 *target)> &&mainPassSetupCallback) const, SetupOcclusionQueryBatch(decltype(OcclusionCulling::QueryBatchBase::npos) maxOcclusion) const;

		private:
			static std::optional<GPUStreamBuffer::Allocator<sizeof(AABB<2>), TerrainVectorQuad::AABB_VB_name>> GPU_AABB_allocator;

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
			TerrainVectorLayer(std::shared_ptr<class Renderer::World> &&world, unsigned int layerIdx, const float (&color)[3], std::string &&layerName);
			~TerrainVectorLayer();
			TerrainVectorLayer(TerrainVectorLayer &) = delete;
			void operator =(TerrainVectorLayer &) = delete;

		public:
			typedef TerrainVectorQuad::ObjectData ObjectData;
			typedef std::unique_ptr<class TerrainVectorQuad, QuadDeleter> QuadPtr;
			QuadPtr AddQuad(unsigned long int vcount, const std::function<void __cdecl(volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const std::function<ObjectData __cdecl(unsigned int objIdx)> &getObjectData);

		private:
			RenderPipeline::RenderStage BuildRenderStage(const FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform, std::function<void (ID3D12GraphicsCommandList2 *target)> &cullPassSetupCallback, std::function<void (ID3D12GraphicsCommandList2 *target)> &mainPassSetupCallback) const;
			RenderPipeline::PipelineStage GetDebugDrawRenderStage() const;

		protected:
			void ScheduleRenderStage(const FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform, std::function<void (ID3D12GraphicsCommandList2 *target)> cullPassSetupCallback, std::function<void (ID3D12GraphicsCommandList2 *target)> mainPassSetupCallback) const;
			void ScheduleDebugDrawRenderStage() const;	// must be after ScheduleRenderStage()
			static void OnFrameFinish() { GPU_AABB_allocator->OnFrameFinish(); }
		};
	}

	class TerrainVectorLayer final : public Impl::TerrainVectorLayer
	{
		friend class Impl::World;
		using Impl::TerrainVectorLayer::TerrainVectorLayer;
#if defined _MSC_VER && _MSC_VER <= 1913 && !defined __clang__
		// this workaround makes '&TerrainVectorLayer::ScheduleRenderStage' accessible from 'Impl::World'\
		somewhat strange as the problem does not reproduce for simple synthetic experiment
		using Impl::TerrainVectorLayer::ScheduleRenderStage;
		using Impl::TerrainVectorLayer::ScheduleDebugDrawRenderStage;
#endif
	};
}