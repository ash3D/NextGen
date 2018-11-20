/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "occlusion tree.h"

using namespace std;
using namespace Renderer;

// occlusion summation: new = old + (1 - old) * increment = old + increment - old * increment
inline unsigned OcclusionTree::OcclusionDelta(unsigned curOcclusion, unsigned occlusionIncrement) noexcept
{
	return occlusionIncrement - curOcclusion * occlusionIncrement / fullOcclusion;
}

template<size_t ...idx>
inline OcclusionTree::OcclusionTree(index_sequence<idx...>) : levels{ tiles, tiles + QuadTreeNodeCount(idx)... }
{}

OcclusionTree::OcclusionTree() : OcclusionTree(make_index_sequence<height>())
{
	tiles[0].parent = nullptr;
	unsigned long tileCount = 4;
	for (unsigned i = 1, stride = 2; i <= height; i++, stride <<= 1, tileCount <<= 2)
		for (unsigned long j = 0; j < tileCount; j++)
		{
			const auto r = j / stride, c = j % stride;
			levels[i][j].parent = levels[i - 1] + r / 2 * (stride >> 1) + c / 2;
		}
}

void OcclusionTree::Tile::Insert(unsigned short occlusion, unsigned short occlusionThreshold)
{
	occlusionThreshold = max<unsigned short>(occlusionThreshold, 1);

	// select insertion layer

	unsigned short insertLayer = childrenPropagatedLayer;
	unsigned accumulatedOcclusion = childrenPropagatedOcclusion;

	assert(tileLayer <= childrenPropagatedLayer);
	__assume(tileLayer <= childrenPropagatedLayer);
	if (tileLayer == childrenPropagatedLayer)
		accumulatedOcclusion += OcclusionDelta(accumulatedOcclusion, tileOcclusion);
	
	// walk up the tree
	for (const Tile *ancestor = parent; ancestor; ancestor = ancestor->parent)
	{
		if (ancestor->tileLayer > insertLayer)
		{
			insertLayer = ancestor->tileLayer;
			accumulatedOcclusion = ancestor->tileOcclusion;
		}
		else if (ancestor->tileLayer == insertLayer)
			accumulatedOcclusion += OcclusionDelta(accumulatedOcclusion, ancestor->tileOcclusion);
	}

	if (accumulatedOcclusion >= occlusionThreshold)
		insertLayer++;

	if (insertLayer > childrenPropagatedLayer)
	{
		tileLayer = childrenPropagatedLayer = insertLayer;	// keep childrenPropagatedLayer at least as tileLayer
		tileOcclusion = occlusion;
		childrenPropagatedOcclusion = 0;
	}
	else
		tileOcclusion += OcclusionDelta(tileOcclusion, occlusion);

	// propagate layer and occlusion up the tree
	for (Tile *ancestor = parent; ancestor; ancestor = ancestor->parent)
	{
		occlusion /= 4;
		if (ancestor->childrenPropagatedLayer < insertLayer)
		{
			// update layer for ancestor
			ancestor->childrenPropagatedLayer = insertLayer;
			ancestor->childrenPropagatedOcclusion = occlusion;
		}
		else if (ancestor->childrenPropagatedLayer == insertLayer)
			ancestor->childrenPropagatedOcclusion += OcclusionDelta(ancestor->childrenPropagatedOcclusion, occlusion);
	}
}

auto OcclusionTree::FindTileForAABBProjection(const AABB<2> &aabb) const -> pair<Tile *, float>
{
	using namespace placeholders;

	// deduce tree level from AABB size
	const auto &size = aabb.Size();
	const unsigned depth = clamp<signed>(log2(2.f / fmax(size.x, size.y)), 0, height), stride = 1u << depth;	// trancate fractional part

	// [-1, +1] -> [0, stride]
	auto center = aabb.Center();
	center += 1.f;
	center *= .5f * stride;

	/*
	clamp position
	alternative to nextafter is to set rounding mode towards 0 and clamp to 'stride - 1'
	it is also possible to clamp after conversion to signed [long [long]] int but undefined behavior possible if out of range occurs
	*/
#if 0
	// TODO: remove references for apply results in vector math
	center = center.apply(bind(clamp<float>, _1, 0.f, nextafter((float)stride, 0.f));
#elif 0
	center = center.apply(bind([](float v, float lo, float hi) { return clamp(v, lo, hi); }, _1, 0.f, nextafter((float)stride, 0.f)));
#else
	center.x = clamp<float>(center.x, 0.f, nextafter((float)stride, 0.f));
	center.y = clamp<float>(center.y, 0.f, nextafter((float)stride, 0.f));
#endif
	HLSL::uint2 tilePos(center);
	assert(all(tilePos < stride));

	// calculate tile coverage
	const float tileSquare = 4.f / (stride * stride);	// sqr(2.f / stride)
	const float tileCoverage = fmin(size.x * size.y / tileSquare, 1.f);	// AABB normally not larger than tile but huge AABBs which falls into 0 tree level can overlap more than the whole screen => clamp to 1.f

	return { levels[depth] + static_cast<unsigned long>(tilePos.y * stride + tilePos.x), tileCoverage };
}