/**
\author		Alexey Shaydurov aka ASH
\date		26.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#define NOMINMAX

#include <cstddef>
#include <memory>
#include <string>
#include <list>
#include <utility>	// for std::forward
#include <functional>
#include <wrl/client.h>
#include "../AABB.h"
#include "../world hierarchy.h"

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12GraphicsCommandList1;
struct D3D12_RANGE;

#if !_DEBUG
#define PERSISTENT_MAPS 1
#endif

extern void __cdecl InitRenderer();

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	class Viewport;
	class TerrainVectorLayer;
	class World;

	namespace Impl
	{
		class Viewport;
		class TerrainVectorLayer;

		using WRL::ComPtr;

		class World : public std::enable_shared_from_this<Renderer::World>
		{
			friend extern void __cdecl ::InitRenderer();

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
			static ComPtr<ID3D12Resource> perFrameCB;
			struct PerFrameData;	// defined in "per-frame data.h" to eliminate dependencies on d3d12.h here

		private:
			static ComPtr<ID3D12Resource> TryCreatePerFrameCB(), CreatePerFrameCB();

		private:
			static volatile struct PerFrameData
#if PERSISTENT_MAPS
				*perFrameCB_CPU_ptr, *TryMapPerFrameCB(),
#endif
				*MapPerFrameCB(const D3D12_RANGE *readRange = NULL);

		private:
			// terrain
			float terrainXform[4][3];
			std::list<Renderer::TerrainVectorLayer, Allocator<Renderer::TerrainVectorLayer>> terrainVectorLayers;

		private:
			class Instance
			{
				AABB<3> aabb;

			public:
				auto GetAABB() const { return aabb; }
			};
			class NodeData
			{
				WRL::ComPtr<ID3D12GraphicsCommandList1> bundle;
			};
			//mutable Hierarchy::BVH<Instance, NodeData, Hierarchy::QUADTREE> bvh;

		protected:
			World(const float (&terrainXform)[4][3]);
			~World();
			World(World &) = delete;
			void operator =(World &) = delete;

		protected:
			void Render(const float (&viewXform)[4][3], const float (&projXform)[4][4], const std::function<void (bool enableRT, ID3D12GraphicsCommandList1 *target)> &setupRenderOutputCallback) const;
			void OnFrameFinish() const;

		private:
			//void ScheduleNode(decltype(bvh)::Node &node) const;

		public:
			std::shared_ptr<Renderer::Viewport> CreateViewport() const;
			std::shared_ptr<Renderer::TerrainVectorLayer> AddTerrainVectorLayer(unsigned int layerIdx, const float (&color)[3], std::string layerName);
			//Instance AddStaticInstance(float x, float y);
		};
	}

	class World final : public Impl::World
	{
		friend std::shared_ptr<World> __cdecl MakeWorld(const float (&terrainXform)[4][3]);
		friend class Impl::Viewport;
		friend class Impl::TerrainVectorLayer;	// for Allocator
		friend class TerrainVectorQuad;			// for Allocator

#if defined _MSC_VER && _MSC_VER <= 1912
	private:
		struct tag {};

	public:
		World(tag, const float(&terrainXform)[4][3]) : Impl::World(terrainXform) {}
#else
	private:
		using Impl::World::World;
		~World() = default;
#endif
	};
}