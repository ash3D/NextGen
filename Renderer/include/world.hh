/**
\author		Alexey Shaydurov aka ASH
\date		26.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#define NOMINMAX

#include <cstddef>
#include <memory>
#include <string>
#include <array>
#include <vector>
#include <list>
#include <utility>	// for std::forward
#include <functional>
#include <optional>
#include <variant>
#include <wrl/client.h>
#include "../tracked resource.h"
#include "../AABB.h"
#include "../world hierarchy.h"
#include "../render stage.h"
#include "../render pipeline.h"
#include "../GPU stream buffer allocator.h"
#include "../SO buffer.h"
#include "../occlusion query batch.h"
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12GraphicsCommandList2;
struct D3D12_RANGE;
struct D3D12_CPU_DESCRIPTOR_HANDLE;

#if !_DEBUG
#define PERSISTENT_MAPS 1
#endif

extern void __cdecl InitRenderer();

namespace Renderer
{
	namespace WRL = Microsoft::WRL;
	namespace HLSL = Math::VectorMath::HLSL;

	class Viewport;
	class TerrainVectorLayer;
	class Object3D;
	class Instance;
	class World;

	namespace Impl
	{
		class Viewport;
		class TerrainVectorLayer;
#if defined _MSC_VER && (_MSC_VER == 1914 || _MSC_VER == 1915)
		class Instance;
#endif
		using WRL::ComPtr;

		class World : public std::enable_shared_from_this<Renderer::World>, RenderPipeline::IRenderStage
		{
			friend extern void __cdecl ::InitRenderer();
			friend struct WorldViewContext;

		protected:
			// custom allocator needed because standard one does not have access to private ctor/dtor
			template<typename T>
			class Allocator : public std::allocator<T>
			{
			public:
				template<class Other>
				struct rebind
				{
					typedef Allocator<Other> other;
				};

			public:
				using std::allocator<T>::allocator;

			public:
				template<class Class, typename ...Params>
				void construct(Class *p, Params &&...params)
				{
					::new((void *)p) Class(std::forward<Params>(params)...);
				}

				template<class Class>
				void destroy(Class *p)
				{
					p->~Class();
				}
			};

		protected:
			// hazard tracking is not needed here - all the waiting required perormed in globalFrameVersioning dtor
			static ComPtr<ID3D12Resource> globalGPUBuffer;
			struct GlobalGPUBufferData;	// defined in "global GPU buffer data.h" to eliminate dependencies on d3d12.h here

		private:
			static ComPtr<ID3D12Resource> TryCreateGlobalGPUBuffer(), CreateGlobalGPUBuffer();

		private:
			static volatile struct GlobalGPUBufferData
#if PERSISTENT_MAPS
				*globalGPUBuffer_CPU_ptr, *TryMapGlobalGPUBuffer(),
#endif
				*MapGlobalGPUBuffer(const D3D12_RANGE *readRange = NULL);

		private:
			// terrain
			float terrainXform[4][3];
			std::list<Renderer::TerrainVectorLayer, Allocator<Renderer::TerrainVectorLayer>> terrainVectorLayers;

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
#if defined _MSC_VER && _MSC_VER <= 1915
				inline const AABB<3> &GetAABB() const noexcept;
#else
				inline const auto &GetAABB() const noexcept;
#endif
				inline unsigned long int GetTriCount() const noexcept;
				float GetOcclusion() const noexcept { return .7f; }
			};

			//class NodeData
			//{
			//	WRL::ComPtr<ID3D12GraphicsCommandList2> bundle;
			//};

		private:
			// static objects
			mutable Hierarchy::BVH<Hierarchy::ENNEATREE, BVHObject> bvh;
			mutable decltype(bvh)::View bvhView;
			mutable std::list<Renderer::Instance, Allocator<Renderer::Instance>> staticObjects;
			mutable TrackedResource<ID3D12Resource> staticObjectsCB;
			struct StaticObjectData;
			void InvalidateStaticObjects();

		private:
			mutable struct WorldViewContext *viewCtx;
			mutable UINT64 tonemapParamsGPUAddress;
			mutable ID3D12Resource *ZBuffer;
			mutable SIZE_T dsv;

#pragma region occlusion query passes
		private:
			static ComPtr<ID3D12RootSignature> xformAABB_RootSig, TryCreateXformAABB_RootSig(), CreateXformAABB_RootSig();
			static ComPtr<ID3D12PipelineState> xformAABB_PSO, TryCreateXformAABB_PSO(), CreateXformAABB_PSO();
			static ComPtr<ID3D12RootSignature> cullPassRootSig, TryCreateCullPassRootSig(), CreateCullPassRootSig();
			static std::array<ComPtr<ID3D12PipelineState>, 2> cullPassPSOs, TryCreateCullPassPSOs(), CreateCullPassPSOs();

		private:
			mutable std::function<void (ID3D12GraphicsCommandList2 *target)> cullPassSetupCallback;
			struct OcclusionQueryGeometry
			{
				ID3D12Resource *VB;
				unsigned long int startIdx, xformedStartIdx;
				unsigned int count;
			};
			mutable std::vector<OcclusionQueryGeometry> queryStream;

		private:
			void XformAABBPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd) const;
			void CullPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, bool final) const;

		private:
			void SetupCullPass(std::function<void (ID3D12GraphicsCommandList2 *target)> &&setupCallback) const;
			void IssueOcclusion(decltype(bvhView)::Node::OcclusionQueryGeometry occlusionQueryGeometry, unsigned long int &counter) const;
#pragma endregion

#pragma region main passes
		private:
			mutable std::function<void (ID3D12GraphicsCommandList2 *target)> mainPassSetupCallback;
			struct RenderData
			{
				const Renderer::Instance *instance;
				decltype(OcclusionCulling::QueryBatchBase::npos) occlusion;
			};
			mutable std::vector<RenderData> renderStreams[2];

		private:
			void MainPassPre(CmdListPool::CmdList &target) const, MainPassPost(CmdListPool::CmdList &target) const;
			void MainPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, bool final) const;

		private:
			void SetupMainPass(std::function<void (ID3D12GraphicsCommandList2 *target)> &&setupCallback) const;
			void IssueObjects(const decltype(bvh)::Node &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion) const;
			bool IssueNodeObjects(const decltype(bvh)::Node &node, decltype(OcclusionCulling::QueryBatchBase::npos) occlusion,  decltype(OcclusionCulling::QueryBatchBase::npos), decltype(bvhView)::Node::Visibility visibility) const;
#pragma endregion

#pragma region visualize occlusion pass
		private:
			static ComPtr<ID3D12RootSignature> AABB_RootSig, TryCreateAABB_RootSig(), CreateAABB_RootSig();
			static std::array<ComPtr<ID3D12PipelineState>, 2> AABB_PSOs, TryCreateAABB_PSOs(), CreateAABB_PSOs();

		private:
			void AABBPassPre(CmdListPool::CmdList &target) const, AABBPassPost(CmdListPool::CmdList &target) const;
			void AABBPassRange(CmdListPool::CmdList &target, unsigned long rangeBegin, unsigned long rangeEnd, bool visible) const;
#pragma endregion

		private:
			void StagePre(CmdListPool::CmdList &target) const, StagePost(CmdListPool::CmdList &target) const;
			void XformAABBPass2CullPass(CmdListPool::CmdList &target) const, CullPass2MainPass(CmdListPool::CmdList &target, bool final) const, MainPass2CullPass(CmdListPool::CmdList &target) const;

		private:
			// order is essential (TRANSIENT, then DUAL), index based access used
			mutable std::variant<OcclusionCulling::QueryBatch<OcclusionCulling::TRANSIENT>, OcclusionCulling::QueryBatch<OcclusionCulling::DUAL>> occlusionQueryBatch;

		private:
			// Inherited via IRenderStage
			virtual void Sync() const override final;

		private:
			RenderPipeline::PipelineItem
				GetStagePre(unsigned int &length) const, GetStagePost(unsigned int &length) const,
				GetXformAABBPassRange(unsigned int &length) const,
				GetFirstCullPassRange(unsigned int &length) const, GetSecondCullPassRange(unsigned int &length) const,
				GetFirstMainPassRange(unsigned int &length) const, GetSecondMainPassRange(unsigned int &length) const,
				GetXformAABBPass2FirstCullPass(unsigned int &length) const, GetFirstCullPass2FirstMainPass(unsigned int &length) const, GetFirstMainPass2SecondCullPass(unsigned int &length) const, GetSecondCullPass2SecondMainPass(unsigned int &length) const,
				GetMainPassPre(unsigned int &length) const, GetMainPassRange(unsigned int &length) const, GetMainPassPost(unsigned int &length) const,
				GetAABBPassPre(unsigned int &length) const, GetAABBPassPost(unsigned int &length) const,
				GetHiddenPassRange(unsigned int &length) const, GetVisiblePassRange(unsigned int &length) const;

		private:
			void Setup(struct WorldViewContext &viewCtx, UINT64 tonemapParamsGPUAddress, ID3D12Resource *ZBuffer, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, std::function<void (ID3D12GraphicsCommandList2 *target)> &&cullPassSetupCallback, std::function<void (ID3D12GraphicsCommandList2 *target)> &&mainPassSetupCallback) const, SetupOcclusionQueryBatch(decltype(OcclusionCulling::QueryBatchBase::npos) maxOcclusion) const;

		private:
			static constexpr const WCHAR AABB_VB_name[] = L"3D objects occlusion query boxes", xformedAABB_SO_name[] = L"3D objects xformed occlusion query boxes";
			static std::optional<GPUStreamBuffer::Allocator<sizeof(AABB<3>), AABB_VB_name>> GPU_AABB_allocator;
			static SOBuffer::Allocator<xformedAABB_SO_name> xformedAABBsStorage;
			mutable SOBuffer::Handle xformedAABBs;
			static constexpr UINT xformedAABBSize = sizeof(float [4])/*corner*/ + sizeof(float [3][4])/*extents*/;

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
			void Render(struct WorldViewContext &viewCtx, const float (&viewXform)[4][3], const float (&projXform)[4][4], UINT64 tonemapParamsGPUAddress, ID3D12Resource *ZBuffer, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, const std::function<void (ID3D12GraphicsCommandList2 *target, bool enableRT)> &setupRenderOutputCallback) const;
			void OnFrameFinish() const;

		public:
			typedef std::unique_ptr<const Renderer::Instance, InstanceDeleter> InstancePtr;
			void SetSunDir(float zenith, float azimuth);
			std::shared_ptr<Renderer::Viewport> CreateViewport() const;
			std::shared_ptr<Renderer::TerrainVectorLayer> AddTerrainVectorLayer(unsigned int layerIdx, const float (&color)[3], std::string layerName);
			InstancePtr AddStaticObject(Renderer::Object3D object, const float (&xform)[4][3], const AABB<3> &worldAABB);
			void FlushUpdates() const;	// const to be able to call from Render()

		private:
			RenderPipeline::RenderStage BuildRenderStage(struct WorldViewContext &viewCtx, const HLSL::float4x4 &frustumXform, const HLSL::float4x3 &viewXform, UINT64 tonemapParamsGPUAddress, ID3D12Resource *ZBuffer, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, std::function<void (ID3D12GraphicsCommandList2 *target)> &cullPassSetupCallback, std::function<void (ID3D12GraphicsCommandList2 *target)> &mainPassSetupCallback) const;
			RenderPipeline::PipelineStage GetDebugDrawRenderStage() const;
		};
	}

	class World final : public Impl::World
	{
		friend std::shared_ptr<World> __cdecl MakeWorld(const float (&terrainXform)[4][3], float zenith = 0, float azimuth = 0);
		friend class Impl::Viewport;
		friend class Impl::TerrainVectorLayer;	// for Allocator
		friend class TerrainVectorQuad;			// for Allocator
#if defined _MSC_VER && (_MSC_VER == 1914 || _MSC_VER == 1915)
		friend class Impl::Instance;			// for Allocator
#endif

#if defined _MSC_VER && _MSC_VER <= 1915
	private:
		struct tag {};

	public:
		World(tag, const float (&terrainXform)[4][3], float zenith, float azimuth) : Impl::World(terrainXform, zenith, azimuth) {}
#else
	private:
		using Impl::World::World;
		~World() = default;
#endif
	};
}