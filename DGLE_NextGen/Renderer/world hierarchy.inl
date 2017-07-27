/**
\author		Alexey Shaydurov aka ASH
\date		27.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "world hierarchy.h"
#include "frustum culling.h"
#include "occlusion query shceduling.h"

namespace Renderer::Impl::Hierarchy
{
	template<class AABB>
	inline bool AABBSizeSeparator<AABB>::operator ()(const AABB &aabb) const
	{
		return all(aabb.Size() > size);
	}

	template<Axis axis>
	template<class AABB>
	inline bool AAABBSplitter<axis>::operator ()(const AABB &aabb) const
	{
		constexpr auto axisIdx = std::underlying_type_t<Axis>(axis);
		return aabb.Center()[axisIdx] < split;
	}

	template<Axis axis>
	template<class AABB>
	inline bool AAABBInternalSplitter<axis>::operator ()(const AABB &aabb) const
	{
		constexpr auto axisIdx = std::underlying_type_t<Axis>(axis);
		return aabb.min[axisIdx] < split && aabb.max[axisIdx] > split;
	}

	template<Axis axis>
	template<class AABB>
	inline void AABBAxisBounds<axis>::operator ()(const AABB &aabb) const
	{
		constexpr auto axisIdx = std::underlying_type_t<Axis>(axis);
		min = fmin(min, aabb.min[axisIdx]);
		max = fmax(max, aabb.max[axisIdx]);
	}

	template<class AABBHandler>
	template<class Object>
	inline auto Object2AABB<AABBHandler>::operator()(const Object &object) const
	{
		return AABBHandler::operator ()(object.GetAABB());
	}

	template<class Object, TreeStructure treeStructure>
	template<std::remove_extent_t<decltype(childrenOrder)> ...idx>
	inline BVH<Object, treeStructure>::Node::Node(std::integer_sequence<std::remove_extent_t<decltype(childrenOrder)>, idx...>) : childrenOrder{idx...}
	{}

	template<class Object, TreeStructure treeStructure>
	BVH<Object, treeStructure>::Node::Node(typename std::enable_if_t<true, decltype(objects)>::iterator srcBegin, typename std::enable_if_t<true, decltype(objects)>::iterator srcEnd, SplitTechnique splitTechnique, ...) :
		Node(std::make_integer_sequence<std::remove_extent_t<decltype(childrenOrder)>, std::extent_v<decltype(childrenOrder)>>())
	{
		using namespace std;

		// calculate AABB and mean pos\
			consider calculating mean pos after big objects being separated
		decltype(aabb.Center()) meanPoint{};
		for_each(srcBegin, srcEnd, [&](const Object &object)
		{
			const auto &curAABB = object.GetAABB();
			aabb.Refit(curAABB);
			meanPoint += curAABB.Center();
		});
		meanPoint /= distance(srcBegin, srcEnd);

		// place big objects into current node
		objBegin = srcBegin;
		objEnd = srcBegin = partition(srcBegin, srcEnd, Object2AABB<AABBSizeSeparator<decltype(aabb)>>(aabb.Size()));

		// split if feasible
		if (distance(srcBegin, srcEnd) > 1)
		{
			const auto splitPoint = [&, splitTechnique]
			{
				switch (splitTechnique)
				{
				case SplitTechnique::REGULAR:
					return aabb.Center();
				case SplitTechnique::MEAN:
					return meanPoint;
				}
			}();

			// consider using C++17 constexpr if
			switch (treeStructure)
			{
			case ENNEATREE:
				SplitEneaTree(srcBegin, srcEnd, splitTechnique, splitPoint);
				break;
			case ICOSEPTREE:
				SplitIcoseptree(srcBegin, srcEnd, splitTechnique, splitPoint);
				break;
			case QUADTREE:
			case OCTREE:
				va_list params;
				va_start(params, splitTechnique);
				const double overlapThreshold = va_arg(params, double);
				va_end(params);
				switch (treeStructure)
				{
				case QUADTREE:
					SplitQuadtree(srcBegin, srcEnd, splitTechnique, splitPoint, overlapThreshold);
					break;
				case OCTREE:
					SplitOctree(srcBegin, srcEnd, splitTechnique, splitPoint, overlapThreshold);
					break;
				}
				break;
			}

			childrenCount = distance(begin(children), remove(begin(children), end(children), nullptr));
		}
		else
		{
			objEnd = srcEnd;
			childrenCount = 0;
		}
	}

	template<class Object, TreeStructure treeStructure>
	inline void BVH<Object, treeStructure>::Node::CreateChildNode(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, double overlapThreshold, unsigned int idxOffset)
	{
		children[idxOffset] = make_unique<Node>(begin, end, splitTechnique, overlapThreshold);
	}

	template<class Object, TreeStructure treeStructure>
	template<Axis axis, class F>
	inline void BVH<Object, treeStructure>::Node::Split2(const F &action, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset)
	{
		using namespace std;

		constexpr auto axisIdx = underlying_type_t<Axis>(axis);
		constexpr unsigned int idxOffsetStride = 1u << axisIdx;

		auto split = partition(begin, end, Object2AABB<AAABBSplitter<axis>>(splitPoint[axisIdx]));
		if (overlapThreshold < 1.)
		{
			float min[2] = { +INFINITY, +INFINITY }, max[2] = { -INFINITY, -INFINITY };
			for_each(begin, split, Object2AABB<AABBAxisBounds<axis>>(min[0], max[0]));
			for_each(split, end, Object2AABB<AABBAxisBounds<axis>>(min[1], max[1]));
			if ((max[0] - min[1]) / fmin(max[0] - min[0], max[1] - min[1]) > overlapThreshold)
				split = end;
		}
		if (split != begin)
			action(begin, split, overlapThreshold, idxOffset);
		if (split != end)
			action(split, end, overlapThreshold, idxOffset + idxOffsetStride);
	}

	template<class Object, TreeStructure treeStructure>
	inline void BVH<Object, treeStructure>::Node::SplitQuadtree(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset)
	{
		using namespace std;
		using namespace placeholders;

		// problem with nested bind
#if 0
		const auto createChildNode = bind(&Node::CreateChildNode, this, _1/*begin*/, _2/*end*/, splitTechnique, _3/*overlapThreshold*/, _4/*idxOffset*/);
#else
		const auto createChildNode = [this, splitTechnique](typename enable_if_t<true, decltype(objects)>::iterator begin, typename enable_if_t<true, decltype(objects)>::iterator end, double overlapThreshold, unsigned int idxOffset)
		{
			CreateChildNode(begin, end, splitTechnique, overlapThreshold, idxOffset);
		};
#endif
		Split2<Axis::Y>(bind(&Node::Split2<Axis::X, decltype(createChildNode)>, this, createChildNode, _1/*begin*/, _2/*end*/, splitPoint, _3/*overlapThreshold*/, _4/*idxOffset*/), begin, end, splitPoint, overlapThreshold, idxOffset);
	}

	// 1 call site
	template<class Object, TreeStructure treeStructure>
	inline void BVH<Object, treeStructure>::Node::SplitOctree(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold)
	{
		using namespace std;
		using namespace placeholders;

		Split2<Axis::Z>(bind(&Node::SplitQuadtree, this, _1/*begin*/, _2/*end*/, splitTechnique, splitPoint, _3/*overlapThreshold*/, _4/*idxOffset*/), begin, end, splitPoint, overlapThreshold);
	}

	template<class Object, TreeStructure treeStructure>
	template<Axis axis, class F>
	void BVH<Object, treeStructure>::Node::Split3(const F &action, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, unsigned int idxOffset)
	{
		using namespace std;

		constexpr auto axisIdx = underlying_type_t<Axis>(axis);
		constexpr unsigned int idxOffsetStride = 1u | axisIdx << axisIdx;	// [1, 3, 9]

		const auto splitInternal = partition(begin, end, Object2AABB<AAABBInternalSplitter<axis>>(splitPoint[axisIdx]));
		if (splitInternal != begin)
			action(begin, splitInternal, idxOffset);
		if (splitInternal != end)
		{
			const auto split = partition(splitInternal, end, Object2AABB<AAABBSplitter<axis>>(splitPoint[axisIdx]));
			if (split != splitInternal)
				action(splitInternal, split, idxOffset += idxOffsetStride);
			if (split != end)
				action(split, end, idxOffset += idxOffsetStride);
		}
	}

	template<class Object, TreeStructure treeStructure>
	inline void BVH<Object, treeStructure>::Node::SplitEneaTree(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, unsigned int idxOffset)
	{
		using namespace std;
		using namespace placeholders;

		const auto createChildNode = [this, splitTechnique](typename enable_if_t<true, decltype(objects)>::iterator begin, typename enable_if_t<true, decltype(objects)>::iterator end, unsigned int idxOffset)
		{
			children[idxOffset] = make_unique<Node>(begin, end, splitTechnique);
		};
		Split3<Axis::Y>(bind(&Node::Split3<Axis::X, decltype(createChildNode)>, this, createChildNode, _1/*begin*/, _2/*end*/, splitPoint, _3/*idxOffset*/), begin, end, splitPoint, idxOffset);
	}

	// 1 call site
	template<class Object, TreeStructure treeStructure>
	inline void BVH<Object, treeStructure>::Node::SplitIcoseptree(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint)
	{
		using namespace std;
		using namespace placeholders;

		Split3<Axis::Z>(bind(&Node::SplitEneaTree, this, _1/*begin*/, _2/*end*/, splitTechnique, splitPoint, _3/*idxOffset*/), begin, end, splitPoint);
	}

	template<class Object, TreeStructure treeStructure>
	void BVH<Object, treeStructure>::Node::TraverseOrdered(const std::function<void (Node &node)> &nodeHandler)
	{
		using namespace std;

		if (visible)
		{
			nodeHandler(*this);

			for_each(cbegin(childrenOrder), next(cbegin(childrenOrder), childrenCount), [](remove_extent_t<decltype(childrenOrder)> idx)
			{
				TraverseOrdered(children[idx], nodeHandler);
			});
		}
	}

	template<class Object, TreeStructure treeStructure>
	void BVH<Object, treeStructure>::Node::TraverseParallel(const std::function<void (Node &node)> &nodeHandler, const std::optional<const FrustumCuller<std::enable_if_t<true, decltype(aabb.Center())>::dimension>> &frustumCuller, const HLSL::float4x3 *depthSortXform, bool parentFullyVisible)
	{
		using namespace std;

		// cull if necessary
		if (!parentFullyVisible)
		{
			if (frustumCuller)
			{
				switch (frustumCuller->Cull<false>(aabb))
				{
				case CullResult::OUTSIDE:
					visible = false;
					break;
				case CullResult::INSIDE:
					parentFullyVisible = true;
					[[fallthrough]];
				case CullResult::UNDETERMINED:
					visible = true;
					break;
				}
			}
			if (!visible)
				return;
		}

		if (childrenCount)
		{
			// consider using thread pool instead of async
			future<void> childrenResults[extent_v<decltype(children)>];
			// launch
			transform(next(cbegin(children)), next(cbegin(children), childrenCount), begin(childrenResults), [&nodeHandler, &frustumCuller, depthSortXform, parentFullyVisible](const remove_extent_t<decltype(children)> &child)
			{
				return async(&Node::TraverseParallel, child.get(), cref(nodeHandler), cref(frustumCuller), depthSortXform, parentFullyVisible);
			});

			// traverse first child in this thread
			children[0]->TraverseParallel(nodeHandler, frustumCuller, depthSortXform, parentFullyVisible);

			// sort if necessary
			if (depthSortXform)
			{
				// xform AABBs to view space
				AABB<3> viewSpaceAABBs[extent_v<decltype(children)>];
				transform(cbegin(children), next(cbegin(children), childrenCount), viewSpaceAABBs, [depthSortXform](const remove_extent_t<decltype(children)> &child)
				{
					return TransformAABB(child->aabb, *depthSortXform);
				});

				// sort by near AABB z (needed for occlusion culling to work properly for nested objects)
				sort(begin(childrenOrder), next(begin(childrenOrder), childrenCount), [&viewSpaceAABBs](remove_extent_t<decltype(childrenOrder)> left, remove_extent_t<decltype(childrenOrder)> right) -> bool
				{
					return viewSpaceAABBs[left].min.z < viewSpaceAABBs[right].min.z;
				});
			}

			// wait (maybe future dtor is sufficient?)
			for_each(begin(childrenResults), next(begin(childrenResults), childrenCount - 1), mem_fn(&future<void>::get));
		}
	}

	// returns accumulated AABB measure
	template<class Object, TreeStructure treeStructure>
	float BVH<Object, treeStructure>::Node::CollectOcclusionQueryBoxes(const Node **boxesBegin, const Node **boxesEnd) const
	{
		using namespace std;

		const auto childrenFilter = [](const remove_extent_t<decltype(children)> &child) { return child->visible && !child->shceduleOcclusionQuery; };
		const auto filteredChildrenCount = count_if(cbegin(children), next(cbegin(children), childrenCount), childrenFilter);
		const auto boxesCount = distance(boxesBegin, boxesEnd);
		const float thisNodeMeasure = aabb.Measure();
		
		if (filteredChildrenCount > 0 && boxesCount >= filteredChildrenCount && GetExclusiveTriCount() <= OcclusionCulling::exclusiveTriCountCullThreshold)
		{
			float accumulatedChildrenMeasure = 0.f;
			const auto collectFromChild = [minBoxesPerNode = boxesCount / filteredChildrenCount, additionalBoxes = boxesCount % filteredChildrenCount, segmentBegin = boxesBegin, &accumulatedChildrenMeasure, childrenFilter](const remove_extent_t<decltype(children)> &child) mutable
			{
				if (childrenFilter(child))
				{
					auto segmentEnd = next(segmentBegin, minBoxesPerNode);
					if (additionalBoxes)
					{
						advance(segmentEnd, 1);
						additionalBoxes--;
					}
					accumulatedChildrenMeasure += child->CollectOcclusionQueryBoxes(segmentBegin, segmentEnd);
					segmentBegin = segmentEnd;
				}
			};
#if _MSC_VER && _MSC_VER <= 1910
			for_each(cbegin(children), next(cbegin(children), childrenCount), collectFromChild);
#else
			for_each_n(cbegin(children), childrenCount, collectFromChild);
#endif

			// return children boxes only if they are smaller than this node's box
			if (accumulatedChildrenMeasure / thisNodeMeasure < OcclusionCulling::accumulatedChildrenMeasureThreshold)
			{
				cullExlusiveObjects = false;
				return accumulatedChildrenMeasure;
			}
		}

		*boxesBegin = this;
		fill(next(boxesBegin), boxesEnd, nullptr);
		cullExlusiveObjects = true;
		return thisNodeMeasure;
	}

	template<class Object, TreeStructure treeStructure>
	BVH<Object, treeStructure>::BVH(SplitTechnique splitTechnique, ...)
	{
		switch (treeStructure)
		{
		case ENNEATREE:
		case ICOSEPTREE:
			root = std::make_unique<Node>(objects.begin(), objects.end(), splitTechnique);
			break;
		case QUADTREE:
		case OCTREE:
			va_list params;
			va_start(params, splitTechnique);
			const double overlapThreshold = va_arg(params, double);
			va_end(params);
			assert(isgreaterequal(overlapThreshold, 0.));
			assert(islessequal(overlapThreshold, 1.));
			root = std::make_unique<Node>(objects.begin(), objects.end(), splitTechnique, overlapThreshold);
			break;
		}
	}

	template<class Object, TreeStructure treeStructure>
	inline void BVH<Object, treeStructure>::TraverseOrdered(const std::function<void (Node &node)> &nodeHandler)
	{
		root->TraverseOrdered(nodeHandler);
	}

	template<class Object, TreeStructure treeStructure>
	inline void BVH<Object, treeStructure>::TraverseParallel(const std::function<void (Node &node)> &nodeHandler, const HLSL::float4x4 *frustumXform, const HLSL::float4x3 *depthSortXform)
	{
		std::optional<const FrustumCuller<decltype(std::declval<Object>().GetAABB().Center())::dimension>> frustumCuller;
		if (frustumXform)
			frustumCuller.emplace(*frustumXform);
		root->TraverseParallel(nodeHandler, frustumCuller, depthSortXform);
	}
}