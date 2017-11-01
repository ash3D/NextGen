/**
\author		Alexey Shaydurov aka ASH
\date		01.11.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <memory>
#include <vector>
#include <list>
#include <functional>
#include "world.hh"	// temp for Allocator
#include "../tracked resource.h"
#include "../AABB.h"
#include "../world hierarchy.h"
#include "../render pipeline.h"
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif

struct ID3D12PipelineState;
struct ID3D12GraphicsCommandList1;
struct ID3D12Resource;

namespace Renderer
{
	namespace HLSL = Math::VectorMath::HLSL;
	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		class World;

		template<unsigned int dimension>
		class FrustumCuller;
	}

	class TerrainVectorLayer final : public std::enable_shared_from_this<TerrainVectorLayer>
	{
		friend class Impl::World;
		friend class TerrainVectorQuad;

	private:
		const std::shared_ptr<class World> world;
		const unsigned int layerIdx;
		std::list<class TerrainVectorQuad, World::Allocator<class TerrainVectorQuad>> quads;
		mutable class CRenderStage final : public Impl::RenderPipeline::IRenderStage
		{
			class COcclusionQueryPass final : public Impl::RenderPipeline::IRenderPass
			{
				std::vector<void *> renderStream;

			private:
				// Inherited via IRenderPass
				virtual unsigned long int Length() const noexcept override { return renderStream.size(); }
				virtual ID3D12PipelineState *GetPSO(unsigned long int i) const override { return nullptr; }
				virtual void operator()(unsigned long int rangeBegin, unsigned long int rangeEnd, ID3D12GraphicsCommandList1 *target) const override {}

			public:
				void Setup();
				void IssueOcclusion(void *occlusion);
			} occlusionQueryPass;

			class CMainPass final : public Impl::RenderPipeline::IRenderPass
			{
				const float color[3];
				WRL::ComPtr<ID3D12PipelineState> PSO;
				std::function<void (ID3D12GraphicsCommandList1 *target)> setupCallback;
				struct RenderData
				{
					unsigned long int startIdx, triCount;
					void *occlusion;
					unsigned long int startQuadIdx;
				};
				struct Quad
				{
					unsigned long int streamEnd;
					ID3D12Resource *VIB;	// no reference/lifetime tracking required, it would induce unnecessary overhead (lifetime tracking leads to mutex locs)
					unsigned long int VB_size, IB_size;
					bool IB32bit;
				};
				std::vector<RenderData> renderStream;
				std::vector<Quad> quads;

			public:
				inline CMainPass(const float (&color)[3], const WRL::ComPtr<ID3D12PipelineState> &PSO);
				inline ~CMainPass();

			private:
				// Inherited via IRenderPass
				virtual unsigned long int Length() const noexcept override { return renderStream.size(); }
				virtual ID3D12PipelineState *GetPSO(unsigned long int i) const override { return PSO.Get(); }
				virtual void operator()(unsigned long int rangeBegin, unsigned long int rangeEnd, ID3D12GraphicsCommandList1 *target) const override;

			public:
				void Setup(std::function<void (ID3D12GraphicsCommandList1 *target)> &&setupCallback);
				void IssueQuad(ID3D12Resource *VIB, unsigned long int VB_size, unsigned long int IB_size, bool IB32bit), IssueCluster(unsigned long int startIdx, unsigned long int triCount, void *occlusion);
			} mainPass;

		public:
			inline CRenderStage(const float (&color)[3], const WRL::ComPtr<ID3D12PipelineState> &mainPSO) : mainPass(color, mainPSO) {}

		private:
			// interface implementation
			unsigned int RenderPassCount() const noexcept override { return 2; }
			const Impl::RenderPipeline::IRenderPass &operator [](unsigned renderPassIdx) const override;

		public:
			void Setup(std::function<void (ID3D12GraphicsCommandList1 *target)> &&mainPassSetupCallback);
			void IssueQuad(ID3D12Resource *VIB, unsigned long int VB_size, unsigned long int IB_size, bool IB32bit);
			template<class Node>
			bool IssueNode(const Node &node, void *&coarseOcclusion, void *&fineOcclusion, bool &cullWholeNodeOverriden);

		private:
			template<class Node>
			void IssueExclusiveObjects(const Node &node, void *occlusion);
			template<class Node>
			void IssueChildren(const Node &node, void *occlusion);
			template<class Node>
			void IssueWholeNode(const Node &node, void *occlusion);
		} renderStage;

	private:
		struct QuadDeleter final
		{
			decltype(quads)::const_iterator quadLocation;

		public:
			void operator ()(class TerrainVectorQuad *quadToRemove) const;
		};

	private:
		TerrainVectorLayer(std::shared_ptr<class World> world, unsigned int layerIdx, const float (&color)[3], WRL::ComPtr<ID3D12PipelineState> &mainPSO);
		~TerrainVectorLayer();
		TerrainVectorLayer(TerrainVectorLayer &) = delete;
		void operator =(TerrainVectorLayer &) = delete;

	public:
		struct ObjectData
		{
			unsigned long int triCount;
			const void *tris;
			const AABB<2> &aabb;
		};
		typedef std::unique_ptr<class TerrainVectorQuad, QuadDeleter> QuadPtr;
		QuadPtr AddQuad(unsigned long int vcount, const std::function<void __cdecl(volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const std::function<ObjectData __cdecl(unsigned int objIdx)> &getObjectData);

	private:
		void Render(ID3D12GraphicsCommandList1 *cmdList) const;
		const Impl::RenderPipeline::IRenderStage *BuildRenderStage(const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform, std::function<void (ID3D12GraphicsCommandList1 *target)> &mainPassSetupCallback) const;
		void ShceduleRenderStage(const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform, std::function<void (ID3D12GraphicsCommandList1 *target)> mainPassSetupCallback) const;
	};

	class TerrainVectorQuad final
	{
		friend class TerrainVectorLayer;

		template<typename>
		friend class World::Allocator;

	private:
		struct Object
		{
			AABB<2> aabb;
			unsigned long int triCount;
			unsigned int idx;

			// interface for BVH
		public:
#if defined _MSC_VER && _MSC_VER <= 1911
			const AABB<2> &GetAABB() const { return aabb; }
#else
			const auto &GetAABB() const { return aabb; }
#endif
			unsigned long int GetTriCount() const noexcept { return triCount; }
			float GetOcclusion() const noexcept { return .7f; }
		};
		struct NodeCluster
		{
			unsigned long int startIdx;
		};
		const std::shared_ptr<TerrainVectorLayer> layer;
		mutable Impl::Hierarchy::BVH<Object, NodeCluster, Impl::Hierarchy::QUADTREE> subtree;
		Impl::TrackedResource VIB;	// Vertex/Index Buffer
		const bool IB32bit;
		const unsigned long int VB_size, IB_size;

	private:
		TerrainVectorQuad(std::shared_ptr<TerrainVectorLayer> layer, unsigned long int vcount, const std::function<void (volatile float verts[][2])> &fillVB, unsigned int objCount, bool IB32bit, const std::function<TerrainVectorLayer::ObjectData (unsigned int objIdx)> &getObjectData);
		~TerrainVectorQuad();
		TerrainVectorQuad(TerrainVectorQuad &) = delete;
		void operator =(TerrainVectorQuad &) = delete;

	private:
		void Dispatch(const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform) const;
		void Shcedule(const Impl::FrustumCuller<2> &frustumCuller, const HLSL::float4x4 &frustumXform) const, Issue() const;
	};
}