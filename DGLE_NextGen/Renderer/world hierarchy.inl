/**
\author		Alexey Shaydurov aka ASH
\date		14.05.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "world hierarchy.h"
#include "frustum culling.h"
#include "occlusion query shceduling.h"

// thread pool based MSVC's std::async implementation can lead to deadlocks during tree traverse, alternative technique needed
/*
0 - disable
1 - async
2 - execution::par
*/
#define MULTITHREADED_TREE_TRAVERSE 2

/*
	sorting by near AABB z needed for occlusion culling to work properly for nested objects
	doing this results in somewhat unexpected results though
	sorting by AABB center z is cheaper and behaves more predictable in some situations but it has it's own inconsistencies (probably due to node overlapping)
	futher research needed
*/
#define SORT_AABB_NEAR_Z 1

namespace Renderer::Impl::Hierarchy
{
	template<class AABB>
	inline bool AABBSizeSeparator<AABB>::operator ()(const AABB &aabb) const
	{
		// '>=' required (not '>') to handle degenerate AABBs correctly
		return all(aabb.Size() >= size);
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

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	BVH<treeStructure, Object, CustomNodeData...>::Node::Node(unsigned long &nodeCounter, typename std::enable_if_t<true, decltype(objects)>::iterator srcBegin, typename std::enable_if_t<true, decltype(objects)>::iterator srcEnd, SplitTechnique splitTechnique, ...) :
		objBegin(srcBegin), objEnd(srcEnd), idx(nodeCounter++)
	{
		using namespace std;

		assert(srcBegin != srcEnd);

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
		objExclusiveSeparator = srcBegin = partition(srcBegin, srcEnd, Object2AABB<AABBSizeSeparator<decltype(aabb)>>(aabb.Size()));

		// try to split if feasible
		if (srcBegin != srcEnd)
		{
			// if there is only single object it should be considered as big and no left to being splitted
			assert(distance(objBegin, objEnd) > 1);

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

			// force to split if there are big objects so that small ones gets their own AABB
			bool splitted = objBegin != objExclusiveSeparator;
			// consider using C++17 constexpr if
			switch (treeStructure)
			{
			case ENNEATREE:
				SplitEneaTree(splitted, nodeCounter, srcBegin, srcEnd, splitTechnique, splitPoint);
				break;
			case ICOSEPTREE:
				SplitIcoseptree(splitted, nodeCounter, srcBegin, srcEnd, splitTechnique, splitPoint);
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
					SplitQuadtree(splitted, nodeCounter, srcBegin, srcEnd, splitTechnique, splitPoint, overlapThreshold);
					break;
				case OCTREE:
					SplitOctree(splitted, nodeCounter, srcBegin, srcEnd, splitTechnique, splitPoint, overlapThreshold);
					break;
				}
				break;
			}

			if (splitted)
				childrenCount = distance(begin(children), remove(begin(children), end(children), nullptr));
			else
				objExclusiveSeparator = objEnd;
		}

		// calculate tri count and occlusion
		{
			tie(exclusiveTriCount, occlusion) = accumulate(objBegin, objExclusiveSeparator, make_pair(0ul, 0.f), [renormalizationFactor = 1.f / aabb.Measure()](auto left, const Object &right)
			{
				left.first += right.GetTriCount();

				// renormalize occlusion and perform increment
				const float renormalizedOcclusionIncrement = right.GetOcclusion() * right.GetAABB().Measure() * renormalizationFactor;
				left.second += fma(-left.second, renormalizedOcclusionIncrement, renormalizedOcclusionIncrement);

				return left;
			});

			inclusiveTriCount = accumulate(cbegin(children), next(cbegin(children), childrenCount), exclusiveTriCount, [](unsigned long int left, const remove_extent_t<decltype(children)> &right) noexcept
			{
				return left + right->GetInclusiveTriCount();
			});

			assert(inclusiveTriCount);
		}
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<typename ...Params>
	inline void BVH<treeStructure, Object, CustomNodeData...>::Node::CreateChildNode(bool splitted, unsigned long &nodeCounter, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, unsigned int idxOffset, Params ...params)
	{
		if (splitted)
			children[idxOffset] = make_unique<Node>(nodeCounter, begin, end, splitTechnique, params...);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<Axis axis, class F>
	inline void BVH<treeStructure, Object, CustomNodeData...>::Node::Split2(const F &action, bool &splitted, unsigned long &nodeCounter, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset)
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
		splitted |= split != begin && split != end;
		if (split != begin)
			action(splitted, nodeCounter, begin, split, overlapThreshold, idxOffset);
		if (split != end)
			action(splitted, nodeCounter, split, end, overlapThreshold, idxOffset + idxOffsetStride);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	inline void BVH<treeStructure, Object, CustomNodeData...>::Node::SplitQuadtree(bool &splitted, unsigned long &nodeCounter, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset)
	{
		using namespace std;
		using namespace placeholders;

		const auto createChildNode = bind(&Node::CreateChildNode<double>, this, _1/*splitted*/, _2/*nodeCounter*/, _3/*begin*/, _4/*end*/, splitTechnique, _6/*idxOffset*/, _5/*overlapThreshold*/);
		Split2<Axis::Y>(bind(&Node::Split2<Axis::X, decltype(cref(createChildNode))>, this, cref(createChildNode), _1/*slpitted*/, _2/*nodeCounter*/, _3/*begin*/, _4/*end*/, splitPoint, _5/*overlapThreshold*/, _6/*idxOffset*/), splitted, nodeCounter, begin, end, splitPoint, overlapThreshold, idxOffset);
	}

	// 1 call site
	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	inline void BVH<treeStructure, Object, CustomNodeData...>::Node::SplitOctree(bool &splitted, unsigned long &nodeCounter, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold)
	{
		using namespace std;
		using namespace placeholders;

		Split2<Axis::Z>(bind(&Node::SplitQuadtree, this, _1/*slpitted*/, _2/*nodeCounter*/, _3/*begin*/, _4/*end*/, splitTechnique, splitPoint, _5/*overlapThreshold*/, _6/*idxOffset*/), splitted, nodeCounter, begin, end, splitPoint, overlapThreshold);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<Axis axis, class F>
	void BVH<treeStructure, Object, CustomNodeData...>::Node::Split3(const F &action, bool &splitted, unsigned long &nodeCounter, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, unsigned int idxOffset)
	{
		using namespace std;

		constexpr auto axisIdx = underlying_type_t<Axis>(axis);
		constexpr unsigned int idxOffsetStride = 1u | axisIdx << axisIdx;	// [1, 3, 9]

		const auto splitInternal = partition(begin, end, Object2AABB<AAABBInternalSplitter<axis>>(splitPoint[axisIdx]));
		splitted |= splitInternal != end;
		if (splitInternal != end)
		{
			const auto split = partition(splitInternal, end, Object2AABB<AAABBSplitter<axis>>(splitPoint[axisIdx]));
			if (split != splitInternal)
				action(splitted, nodeCounter, splitInternal, split, idxOffset + idxOffsetStride);
			if (split != end)
				action(splitted, nodeCounter, split, end, idxOffset + 2 * idxOffsetStride);
		}
		if (splitInternal != begin)
			action(splitted, nodeCounter, begin, splitInternal, idxOffset);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	inline void BVH<treeStructure, Object, CustomNodeData...>::Node::SplitEneaTree(bool &splitted, unsigned long &nodeCounter, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, unsigned int idxOffset)
	{
		using namespace std;
		using namespace placeholders;

		const auto createChildNode = bind(&Node::CreateChildNode<>, this, _1/*splitted*/, _2/*nodeCounter*/, _3/*begin*/, _4/*end*/, splitTechnique, _5/*idxOffset*/);
		Split3<Axis::Y>(bind(&Node::Split3<Axis::X, decltype(cref(createChildNode))>, this, cref(createChildNode), _1/*splitted*/, _2/*nodeCounter*/, _3/*begin*/, _4/*end*/, splitPoint, _5/*idxOffset*/), splitted, nodeCounter, begin, end, splitPoint, idxOffset);
	}

	// 1 call site
	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	inline void BVH<treeStructure, Object, CustomNodeData...>::Node::SplitIcoseptree(bool &splitted, unsigned long &nodeCounter, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint)
	{
		using namespace std;
		using namespace placeholders;

		Split3<Axis::Z>(bind(&Node::SplitEneaTree, this, _1/*splitted*/, _2/*nodeCounter*/, _3/*begin*/, _4/*end*/, splitTechnique, splitPoint, _5/*idxOffset*/), splitted, nodeCounter, begin, end, splitPoint);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<typename ...Args, typename NodeHandler, typename ReorderProvider>
	void BVH<treeStructure, Object, CustomNodeData...>::Node::Traverse(NodeHandler &nodeHandler, ReorderProvider reorderProvider, Args ...args)
	{
		using namespace std;

		if (nodeHandler(*this, args...))
		{
			const auto reorder = reorderProvider(*this);
			for (unsigned char i = 0; i < childrenCount; i++)
				children[reorder(i)]->Traverse(nodeHandler, reorderProvider, args...);
		}
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<bool enableEarlyOut, class Allocator>
	std::pair<unsigned long int, bool> BVH<treeStructure, Object, CustomNodeData...>::Node::Schedule(View &view, Allocator &GPU_AABB_allocator, const FrustumCuller<std::enable_if_t<true, decltype(aabb.Center())>::dimension> &frustumCuller, const HLSL::float4x4 &frustumXform, const HLSL::float4x3 *depthSortXform,
		bool parentInsideFrustum, float parentOcclusionCulledProjLength, float parentOcclusion)
	{
		using namespace std;

		typedef View::Node::Visibility Visibility;
		auto &viewData = view.nodes[idx];

		viewData.occlusionQueryGeometry = nullptr;

		// cull if necessary
		if (!parentInsideFrustum)
		{
			switch (frustumCuller.Cull<false>(aabb))
			{
			case CullResult::OUTSIDE:
				viewData.visibility = Visibility::Culled;
				return { GetInclusiveTriCount(), false };
			case CullResult::INSIDE:
				parentInsideFrustum = true;
				break;
			}
		}

		unsigned long int childrenCulledTris = 0;
		bool childQueryCanceled = false;

		const auto traverseChildren = [&]
		{
			if (childrenCount)
			{
#if MULTITHREADED_TREE_TRAVERSE == 0 || MULTITHREADED_TREE_TRAVERSE == 2
				for_each_n(
#if MULTITHREADED_TREE_TRAVERSE
					execution::par,
#endif
					cbegin(children),
#if defined _MSC_VER && _MSC_VER <= 1913 && MULTITHREADED_TREE_TRAVERSE == 2
					(unsigned int)
#endif
					childrenCount,
					[&, depthSortXform, parentInsideFrustum, parentOcclusionCulledProjLength, parentOcclusion](const remove_extent_t<decltype(children)> &child)
				{
					const auto childResult = child->Schedule<enableEarlyOut>(/*nodeHandler, */view, GPU_AABB_allocator, frustumCuller, frustumXform, depthSortXform, parentInsideFrustum, parentOcclusionCulledProjLength, parentOcclusion);
					childrenCulledTris += childResult.first;
					childQueryCanceled |= childResult.second;
				});
#elif MULTITHREADED_TREE_TRAVERSE == 1
				// consider using thread pool instead of async
				future<pair<unsigned long int, bool>> childrenResults[extent_v<decltype(children)>];
				// launch
				transform(next(cbegin(children)), next(cbegin(children), childrenCount), begin(childrenResults), [=, /*&nodeHandler, */&frustumCuller, &frustumXform](const remove_extent_t<decltype(children)> &child)
				{
					return async(&Node::Schedule<enableEarlyOut>, child.get(), /*cref(nodeHandler), */ref(view), ref(GPU_AABB_allocator), cref(frustumCuller), cref(frustumXform), depthSortXform, parentInsideFrustum, parentOcclusionCulledProjLength, parentOcclusion);
				});

				// traverse first child in this thread
				tie(childrenCulledTris, childQueryCanceled) = children[0]->Schedule<enableEarlyOut>(/*nodeHandler, */GPU_AABB_allocator, frustumCuller, frustumXform, depthSortXform, parentInsideFrustum, parentOcclusionCulledProjLength, parentOcclusion);
#else
#error invalid MULTITHREADED_TREE_TRAVERSE value
#endif

				// sort if necessary
				if (depthSortXform)
				{
					// xform AABB Z to view space
					float viewSpaceZ[extent_v<decltype(children)>];
					transform(cbegin(children), next(cbegin(children), childrenCount), viewSpaceZ, [depthSortXform](const remove_extent_t<decltype(children)> &child) -> float
					{
#if SORT_AABB_NEAR_Z

						return TransformAABB(child->aabb, *depthSortXform).min.z;
#else
						return mul(child->aabb.Center(), *depthSortXform).z;
#endif
					});

					// sort by view space Z
					sort(begin(viewData.childrenOrder), next(begin(viewData.childrenOrder), childrenCount), [&viewSpaceZ](remove_extent_t<decltype(viewData.childrenOrder)> left, remove_extent_t<decltype(viewData.childrenOrder)> right) -> bool
					{
						return viewSpaceZ[left] < viewSpaceZ[right];
					});
				}

#if MULTITHREADED_TREE_TRAVERSE == 1
				for_each_n(begin(childrenResults), childrenCount - 1, [&childrenCulledTris, &childQueryCanceled](remove_extent_t<decltype(childrenResults)> &childResult)
				{
					const auto resolvedResult = childResult.get();
					childrenCulledTris += resolvedResult.first;
					childQueryCanceled |= resolvedResult.second;
				});
#endif
			}

			viewData.visibility = childrenCulledTris ? Visibility::Composite : Visibility::Atomic;
		};

		if (OcclusionCulling::EarlyOut(GetInclusiveTriCount()))
		{
			// parentInsideFrustum now relates to this node
			if (enableEarlyOut && parentInsideFrustum)
				viewData.visibility = Visibility::Atomic;
			else
				traverseChildren();
		}
		else
		{
			// pre
#if 1
			const auto clipSpaceAABB = MakeClipSpaceAABB(frustumXform, GetAABB());
#else
			const ClipSpaceAABB clipSpaceAABB(frustumXform, GetAABB());
#endif
			const AABB<3> NDCSpaceAABB(clipSpaceAABB);
			const HLSL::float2 aabbProjSize = NDCSpaceAABB.Size();
			const float aabbProjSquare = aabbProjSize.x * aabbProjSize.y;
			const float aabbProjLength = fmax(aabbProjSize.x, aabbProjSize.y);
			// TODO: replace 'z >= 0 && w > 0' with 'w >= znear' and use 2D NDC space AABB
			bool cancelQueryDueToParent = false, scheduleOcclusionQuery = NDCSpaceAABB.min.z >= 0.f && clipSpaceAABB.MinW() > 0.f && OcclusionCulling::QueryBenefit<false>(aabbProjSquare, GetInclusiveTriCount()) &&
				!(cancelQueryDueToParent = (parentOcclusionCulledProjLength <= OcclusionCulling::nodeProjLengthThreshold || aabbProjLength / parentOcclusionCulledProjLength >= OcclusionCulling::nestedNodeProjLengthShrinkThreshold) && parentOcclusion < OcclusionCulling::parentOcclusionThreshold);
			if (scheduleOcclusionQuery)
			{
				parentOcclusionCulledProjLength = aabbProjLength;
				parentOcclusion = GetOcclusion();
			}
			else
				parentOcclusion += GetOcclusion() - parentOcclusion * GetOcclusion();

			traverseChildren();

			// post
			assert(!(scheduleOcclusionQuery && cancelQueryDueToParent));
			__assume(!(scheduleOcclusionQuery && cancelQueryDueToParent));
			/*
			scheduleOcclusionQuery == true (=> cancelQueryDueToParent == false)									|	reevaluate scheduleOcclusionQuery if childQueryCanceled == false, otherwise keep scheduled unconditionally
			cancelQueryDueToParent == true (=> scheduleOcclusionQuery == false) && childQueryCanceled == false	|	reevaluate cancelQueryDueToParent and propagate it as childQueryCanceled
			scheduleOcclusionQuery == false && childQueryCanceled == true										|	propagate childQueryCanceled == true unconditionally
			*/
			if (scheduleOcclusionQuery || cancelQueryDueToParent && !childQueryCanceled)
			{
				const unsigned long int restTris = GetInclusiveTriCount() - childrenCulledTris;
				bool queryNeeded = childQueryCanceled || OcclusionCulling::QueryBenefit<true>(aabbProjSquare, restTris);
				if (queryNeeded)
				{
					const Node *boxes[OcclusionCulling::maxOcclusionQueryBoxes];
					const unsigned long int exludedTris = CollectOcclusionQueryBoxes<enableEarlyOut>(view, begin(boxes), end(boxes)).first;
					// reevaluate query benefit after excluding cheap objects during box collection
					if (queryNeeded = childQueryCanceled || OcclusionCulling::QueryBenefit<true>(aabbProjSquare, restTris - exludedTris))
					{
						childQueryCanceled = cancelQueryDueToParent;	// propagate if 'cancelQueryDueToParent == true', reset to false otherwise (scheduleOcclusionQuery == true)
						if (scheduleOcclusionQuery)
						{
							childrenCulledTris = GetInclusiveTriCount();	// ' - exludedTris' ?
							const auto boxesEnd = remove(begin(boxes), end(boxes), nullptr);
							tie(viewData.occlusionQueryGeometry.VB, viewData.occlusionQueryGeometry.startIdx) = GPU_AABB_allocator.Allocate(viewData.occlusionQueryGeometry.count = distance(begin(boxes), boxesEnd));
							// TODO: use persistent maps for release builds as optimization
							CD3DX12_RANGE range(viewData.occlusionQueryGeometry.startIdx * sizeof aabb, viewData.occlusionQueryGeometry.startIdx * sizeof aabb);
							// volatile requires corresponding overloads for AABB and vector math classes assignment
							/*volatile*/ decltype(aabb) *VB_CPU_ptr;
							CheckHR(viewData.occlusionQueryGeometry.VB->Map(0, &range, reinterpret_cast<void **>(/*const_cast<decltype(aabb) **>*/(&VB_CPU_ptr))));
#if defined _MSC_VER && _MSC_VER <= 1913
							transform(begin(boxes), boxesEnd, VB_CPU_ptr + viewData.occlusionQueryGeometry.startIdx, [](const Node *box) noexcept { return box->aabb; });
#else
							transform(begin(boxes), boxesEnd, VB_CPU_ptr + viewData.occlusionQueryGeometry.startIdx, [](remove_extent_t<decltype(boxes)> box) noexcept { return box->aabb; });
#endif
							range.End += viewData.occlusionQueryGeometry.count * sizeof aabb;
							viewData.occlusionQueryGeometry.VB->Unmap(0, &range);	// exception safety (RAII) is not critical for Unmap as it used for tools and debug layer and Maps are ref-counted
						}
					}
				}
			}
		}

		return { childrenCulledTris, childQueryCanceled };
	}

	// returns <exluded tris, accumulated AABB measure>
	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<bool enableEarlyOut>
	std::pair<unsigned long int, float> BVH<treeStructure, Object, CustomNodeData...>::Node::CollectOcclusionQueryBoxes(const View &view, const Node **boxesBegin, const Node **boxesEnd)
	{
		using namespace std;

		typedef View::Node::Visibility Visibility;
		typedef View::Node::OcclusionCullDomain OcclusionCullDomain;
		auto &viewData = view.nodes[idx];

		const auto childrenFilter = [parentAtomic = viewData.visibility == Visibility::Atomic, &viewNodes = view.nodes](const remove_extent_t<decltype(children)> &child)
		{
			return parentAtomic || viewNodes[child->idx].visibility != Visibility::Culled && !viewNodes[child->idx].occlusionQueryGeometry;
		};
		const auto filteredChildrenCount = count_if(cbegin(children), next(cbegin(children), childrenCount), childrenFilter);
		const auto boxesCount = distance(boxesBegin, boxesEnd);
		const float thisNodeMeasure = aabb.Measure();
		
		if (filteredChildrenCount > 0 && boxesCount >= filteredChildrenCount && GetExclusiveTriCount() <= OcclusionCulling::exclusiveTriCountCullThreshold)
		{
			unsigned long int excludedTris = GetExclusiveTriCount();
			float accumulatedChildrenMeasure = 0.f;
			const auto collectFromChild = [&, childrenFilter, minBoxesPerNode = boxesCount / filteredChildrenCount, additionalBoxes = boxesCount % filteredChildrenCount, segmentBegin = boxesBegin]
			(const remove_extent_t<decltype(children)> &child) mutable
			{
				if (childrenFilter(child))
				{
					assert(viewData.visibility != Visibility::Culled);

					if constexpr (enableEarlyOut)
					{
						auto &childViewData = view.nodes[child->idx];

						// reset 'culled' bit which can potetially be set in previous frame and not updated yet during Schedule() due to early out
						reinterpret_cast<underlying_type_t<Visibility> &>(childViewData.visibility) &= 0b01;

						// ensure Atomic visibility propagated for early out nodes
						reinterpret_cast<underlying_type_t<Visibility> &>(childViewData.visibility) |= underlying_type_t<Visibility>(viewData.visibility);

						/*
						'childrenFilter' guarantees that it is either required to clear 'occlusionQueryGeometry' or it is already cleared (=> additional clear here has not effect)
						so additional check is not necessary and is can only serve as optimization to avoid redundant clear
						but clear itself is currently cheap and additional chek would probably be an anti-optimization
						another more costly clear implementation though can potentially benefit from additional check
						*/
#if 0
						if (viewData.visibility == Visibility::Atomic)
#endif
							childViewData.occlusionQueryGeometry = nullptr;	// need to set here because it may not be set in Schedule() due to early out
					}

					auto segmentEnd = next(segmentBegin, minBoxesPerNode);
					if (additionalBoxes)
					{
						++segmentEnd;
						--additionalBoxes;
					}

					const auto collectResults = child->CollectOcclusionQueryBoxes<enableEarlyOut>(view, segmentBegin, segmentEnd);
					excludedTris += collectResults.first;
					accumulatedChildrenMeasure += collectResults.second;

					segmentBegin = segmentEnd;
				}
			};
			for_each_n(cbegin(children), childrenCount, collectFromChild);

			// return children boxes only if they are smaller than this node's box
			if (accumulatedChildrenMeasure / thisNodeMeasure < OcclusionCulling::accumulatedChildrenMeasureShrinkThreshold)
			{
				viewData.occlusionCullDomain = excludedTris ?
					excludedTris == GetExclusiveTriCount() ? OcclusionCullDomain::ChildrenOnly : OcclusionCullDomain::ForceComposite
					: OcclusionCullDomain::WholeNode;
				return { excludedTris, accumulatedChildrenMeasure };
			}
		}

		*boxesBegin = this;
		fill(next(boxesBegin), boxesEnd, nullptr);
		viewData.occlusionCullDomain = OcclusionCullDomain::WholeNode;
		return { 0ul, thisNodeMeasure };
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<typename Iterator>
	BVH<treeStructure, Object, CustomNodeData...>::BVH(Iterator objBegin, Iterator objEnd, SplitTechnique splitTechnique, ...) : objects(objBegin, objEnd)
	{
		switch (treeStructure)
		{
		case ENNEATREE:
		case ICOSEPTREE:
			root = std::make_unique<Node>(nodeCount, objects.begin(), objects.end(), splitTechnique);
			break;
		case QUADTREE:
		case OCTREE:
			va_list params;
			va_start(params, splitTechnique);
			const double overlapThreshold = va_arg(params, double);
			va_end(params);
			assert(isgreaterequal(overlapThreshold, 0.));
			assert(islessequal(overlapThreshold, 1.));
			root = std::make_unique<Node>(nodeCount, objects.begin(), objects.end(), splitTechnique, overlapThreshold);
			break;
		}
	}

	// nodeHandler returns false to stop traversal
	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<typename ...Args, typename F>
	inline void BVH<treeStructure, Object, CustomNodeData...>::Traverse(F &nodeHandler, const Args &...args)
	{
		root->Traverse(nodeHandler, [](const Node &) { return [](unsigned char i) { return i; }; }, args...);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	void BVH<treeStructure, Object, CustomNodeData...>::FreeObjects()
	{
		objects.clear();
		objects.shrink_to_fit();
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	void BVH<treeStructure, Object, CustomNodeData...>::Reset()
	{
		FreeObjects();
		root.reset();
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	inline View<treeStructure, Object, CustomNodeData...>::Node::Node()
	{
		using namespace std;
		iota(begin(childrenOrder), end(childrenOrder), 0u);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	inline auto View<treeStructure, Object, CustomNodeData...>::Node::GetVisibility(OcclusionCullDomain override) const noexcept -> Visibility
	{
		return Visibility(std::underlying_type_t<Visibility>(visibility) & std::underlying_type_t<OcclusionCullDomain>(override));
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	inline void View<treeStructure, Object, CustomNodeData...>::Node::OverrideOcclusionCullDomain(OcclusionCullDomain &overriden) const noexcept
	{
		// ChildrenOnly -> ForceComposite is senseless
		assert(visibility == Visibility::Culled || overriden != OcclusionCullDomain::ChildrenOnly || occlusionCullDomain != OcclusionCullDomain::ForceComposite);
		reinterpret_cast<std::underlying_type_t<OcclusionCullDomain> &>(overriden) |= std::underlying_type_t<OcclusionCullDomain>(occlusionCullDomain);	// strict aliasing rules violation?
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<typename IssueOcclusion, typename IssueObjects>
	bool View<treeStructure, Object, CustomNodeData...>::Node::Issue(
		const typename BVH::Node &bvhNode, const IssueOcclusion &issueOcclusion, const IssueObjects &issueObjects,
		std::remove_const_t<decltype(OcclusionCulling::QueryBatchBase::npos)> &occlusionProvider,
		std::remove_const_t<decltype(OcclusionCulling::QueryBatchBase::npos)> &coarseOcclusion,
		std::remove_const_t<decltype(OcclusionCulling::QueryBatchBase::npos)> &fineOcclusion,
		OcclusionCullDomain &occlusionCullDomainOverriden) const
	{
		if (occlusionQueryGeometry)
		{
			occlusionCullDomainOverriden = occlusionCullDomain;
			issueOcclusion(occlusionQueryGeometry);
			fineOcclusion = ++occlusionProvider;
		}
		else if (fineOcclusion != OcclusionCulling::QueryBatchBase::npos)
			OverrideOcclusionCullDomain(occlusionCullDomainOverriden);
		if (occlusionCullDomainOverriden == OcclusionCullDomain::WholeNode)
			coarseOcclusion = fineOcclusion;
		return issueObjects(bvhNode, coarseOcclusion, fineOcclusion, GetVisibility(occlusionCullDomainOverriden));
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	View<treeStructure, Object, CustomNodeData...>::View(const BVH &bvh) :
		bvh(&bvh), nodes(std::make_unique<Node []>(bvh.nodeCount))
	{}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<typename ...Args, typename F>
	inline void View<treeStructure, Object, CustomNodeData...>::Traverse(F &nodeHandler, const Args &...args) const
	{
		const auto nodeHandlerWrapper = [&](const BVH::Node &bvhNode, Args &...args)
		{
			return nodeHandler(bvhNode, nodes[bvhNode.idx], args...);
		};
		const auto reodredProvider = [this](const BVH::Node &bvhNode)
		{
			return [&](unsigned char i) { return nodes[bvhNode.idx].childrenOrder[i]; };
		};
		bvh->root->Traverse(nodeHandlerWrapper, reodredProvider, args...);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<bool enableEarlyOut, class Allocator>
	inline void View<treeStructure, Object, CustomNodeData...>::Schedule(Allocator &GPU_AABB_allocator, const FrustumCuller<std::enable_if_t<true, decltype(std::declval<Object>().GetAABB().Center())>::dimension> &frustumCuller, const HLSL::float4x4 &frustumXform, const HLSL::float4x3 *depthSortXform)
	{
		bvh->root->Schedule<enableEarlyOut>(*this, GPU_AABB_allocator, frustumCuller, frustumXform, depthSortXform);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	template<typename IssueOcclusion, typename IssueObjects>
	inline void View<treeStructure, Object, CustomNodeData...>::Issue(const IssueOcclusion &issueOcclusion, const IssueObjects &issueObjects, std::remove_const_t<decltype(OcclusionCulling::QueryBatchBase::npos)> &occlusionProvider) const
	{
		using namespace std;
		using namespace placeholders;

		const auto issueNode = bind(&Node::Issue<IssueOcclusion, IssueObjects>, _2, _1, cref(issueOcclusion), cref(issueObjects), ref(occlusionProvider), _3, _4, _5);
		Traverse(issueNode, OcclusionCulling::QueryBatchBase::npos, OcclusionCulling::QueryBatchBase::npos, Node::OcclusionCullDomain::ChildrenOnly);
	}

	template<TreeStructure treeStructure, class Object, class ...CustomNodeData>
	inline void View<treeStructure, Object, CustomNodeData...>::Reset()
	{
		nodes.reset();
	}
}

#undef MULTITHREADED_TREE_TRAVERSE
#undef SORT_AABB_NEAR_Z