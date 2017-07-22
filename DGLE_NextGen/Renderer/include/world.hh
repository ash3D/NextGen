/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <memory>
#include <list>
#include <utility>	// for std::forward
#include <wrl/client.h>
#include "../AABB.h"
#include "../world hierarchy.h"

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12GraphicsCommandList1;

#if !_DEBUG
#define PERSISTENT_MAPS 1
#endif

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	class Viewport;
	class TerrainVectorLayer;
	class World;

	namespace Impl
	{
		class Viewport;

		using WRL::ComPtr;

		class World : public std::enable_shared_from_this<Renderer::World>
		{
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

		private:
			ComPtr<ID3D12RootSignature>	terrainBaseRootSig, terrainVectorLayerRootSig;
			ComPtr<ID3D12PipelineState>	terrainBasePSO, terrainVectorLayerPSO;
			ComPtr<ID3D12Resource>		terrainCB;
#if PERSISTENT_MAPS
			volatile void				*terrainCB_CPU_ptr;
#endif
			float terrainXform[4][3];
			std::list<TerrainVectorLayer, Allocator<TerrainVectorLayer>> terrainVectorLayers;

		private:
			class Instance
			{
				AABB<3> aabb;

			public:
				auto GetAABB() const { return aabb; }
			};
			mutable Hierarchy::BVH<Instance, Hierarchy::QUADTREE> bvh;

		protected:
			World(const float (&terrainXform)[4][3]);
			~World();
			World(World &) = delete;
			void operator =(World &) = delete;

		protected:
			void Render(const float (&viewXform)[4][3], const float (&projXform)[4][4], ID3D12GraphicsCommandList1 *cmdList, unsigned overlapIdx) const;

		private:
			void ScheduleNode(decltype(bvh)::Node &node) const;

		public:
			std::shared_ptr<Renderer::Viewport> CreateViewport() const;
			std::shared_ptr<TerrainVectorLayer> AddTerrainVectorLayer(unsigned int layerIdx, const float (&color)[3]);
			//Instance AddStaticInstance(float x, float y);
		};
	}

	class World final : public Impl::World
	{
		friend std::shared_ptr<World> __cdecl MakeWorld(const float (&terrainXform)[4][3]);
		friend class Impl::Viewport;
		friend class TerrainVectorLayer;	// for Allocator
		friend class TerrainVectorQuad;		// for Allocator

#if defined _MSC_VER && _MSC_VER <= 1910
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