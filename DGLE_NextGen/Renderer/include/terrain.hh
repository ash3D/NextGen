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
#include <functional>
#include <wrl/client.h>
#include "world.hh"	// temp for Allocator
#include "../AABB.h"

struct ID3D12Resource;
struct ID3D12GraphicsCommandList1;

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		class World;
	}

	class TerrainVectorLayer final : public std::enable_shared_from_this<TerrainVectorLayer>
	{
		friend class Impl::World;
		//friend class TerrainVectorQuad;

	private:
		const std::shared_ptr<class World> world;
		const unsigned int layerIdx;
		const float color[3];
		std::list<class TerrainVectorQuad, World::Allocator<class TerrainVectorQuad>> quads;

	private:
		struct QuadDeleter final
		{
			decltype(quads)::const_iterator quadLocation;

		public:
			void operator ()(class TerrainVectorQuad *quadToRemove) const;
		};

	private:
		TerrainVectorLayer(std::shared_ptr<class World> world, unsigned int layerIdx, const float (&color)[3]);
		~TerrainVectorLayer() = default;
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
		QuadPtr AddQuad(unsigned long int vcount, std::function<void __cdecl(volatile float verts[][2])> fillVB, unsigned int objCount, bool IB32bit, std::function<ObjectData __cdecl(unsigned int objIdx)> getObjectData);

	private:
		void Render(ID3D12GraphicsCommandList1 *cmdList) const;
	};

	class TerrainVectorQuad final
	{
		friend class TerrainVectorLayer;

		template<typename>
		friend class World::Allocator;

	private:
		struct Cluster
		{
			AABB<2> aabb;
			unsigned long int startIdx, idxCount;

		public:
			auto GetAABB() const { return aabb; }
		};
		const std::shared_ptr<TerrainVectorLayer> layer;
		std::vector<Cluster> clusters;
		std::vector<unsigned long int> objIdxCounts;	// stores indices count (tricount * 3) for each object
		decltype(objIdxCounts)::value_type idxCount = 0;
		WRL::ComPtr<ID3D12Resource> VIB;	// Vertex/Index Buffer
		unsigned long int VB_size, IB_size;
		const bool IB32bit;

	private:
		TerrainVectorQuad(std::shared_ptr<TerrainVectorLayer> layer, unsigned long int vcount, std::function<void (volatile float verts[][2])> fillVB, unsigned int objCount, bool IB32bit, std::function<TerrainVectorLayer::ObjectData (unsigned int objIdx)> getObjectData);
		~TerrainVectorQuad();
		TerrainVectorQuad(TerrainVectorQuad &) = delete;
		void operator =(TerrainVectorQuad &) = delete;

	private:
		void Render(ID3D12GraphicsCommandList1 *cmdList) const;
	};
}