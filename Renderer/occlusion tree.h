/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "AABB.h"

namespace Renderer
{
	namespace HLSL = Math::VectorMath::HLSL;

	inline constexpr unsigned long int QuadTreeNodeCount(unsigned int treeHeight)
	{
		unsigned long int curDepthNodeCount = 1, result = curDepthNodeCount;
		while (treeHeight--)
			result += curDepthNodeCount *= 4;
		return result;
	}

	// screen space loose quadtree for layer distribution for occlusion query scheduling
	class OcclusionTree
	{
		static constexpr unsigned int height = 8;
		static inline unsigned OcclusionDelta(unsigned curOcclusion, unsigned occlusionIncrement) noexcept;

	public:
		static constexpr unsigned fullOcclusion = UINT_MAX >= 0xffffffff ? 01'00000 : 02'00;	// power of 2 for fast divisions
		class Tile
		{
			friend class OcclusionTree;

		private:
			Tile *parent;
			unsigned short int tileLayer = 0, childrenPropagatedLayer = 0;			// keep childrenPropagatedLayer >= tileLayer
			unsigned short int tileOcclusion = 0, childrenPropagatedOcclusion = 0;	// for current layer only, need to reset when layer is updated

		public:
			void Insert(unsigned short occlusion, unsigned short occlusionThreshold = USHRT_MAX);
		};
		
	private:
		Tile tiles[QuadTreeNodeCount(height)], *const levels[height + 1];

	private:
		template<size_t ...idx>
		inline OcclusionTree(std::index_sequence<idx...>);

	public:
		OcclusionTree();
		OcclusionTree(OcclusionTree &&) = default;
		OcclusionTree &operator =(OcclusionTree &&) = default;

	public:
		/*			  tile coverage											[-1, +1]
							^													^
							|													|	*/
		std::pair<Tile *, float> FindTileForAABBProjection(const AABB<2> &screenSpaceAABB) const;
	};
}