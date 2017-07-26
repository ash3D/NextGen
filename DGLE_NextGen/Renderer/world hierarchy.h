/**
\author		Alexey Shaydurov aka ASH
\date		26.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <utility>
#include <type_traits>
#include <memory>
#include <vector>
#include <functional>
#include <optional>
#include <wrl/client.h>
#undef min
#undef max
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif

struct ID3D12CommandList;

namespace Renderer::Impl
{
	template<unsigned int dimension>
	class FrustumCuller;
}

namespace Renderer::Impl::Hierarchy
{
	namespace WRL = Microsoft::WRL;
	namespace HLSL = Math::VectorMath::HLSL;

	enum TreeStructure
	{
		QUADTREE = 4,
		OCTREE = 8,
		ENNEATREE = 9,
		ICOSEPTREE = 27,
	};

	enum class SplitTechnique
	{
		REGULAR,
		MEAN,
	};

	enum class Axis : unsigned { X, Y, Z };

	// use C++17 class template deduction when it will be supported
	template<class AABB>
	class AABBSizeSeparator
	{
		const decltype(std::declval<AABB>().Size()) size;

	public:
		AABBSizeSeparator(const decltype(size) &size) : size(size * .5f) {}

	public:
		inline bool operator ()(const AABB &aabb) const;
	};

	class AABBSplitterBase
	{
	protected:
		const float split;

	public:
		AABBSplitterBase(float split) : split(split) {}
	};

	template<Axis axis>
	class AAABBSplitter : public AABBSplitterBase
	{
		using AABBSplitterBase::AABBSplitterBase;

	public:
		template<class AABB>
		inline bool operator ()(const AABB &aabb) const;
	};

	// for ENNEATREE/ICOSEPTREE
	template<Axis axis>
	class AAABBInternalSplitter : public AABBSplitterBase
	{
		using AABBSplitterBase::AABBSplitterBase;

	public:
		template<class AABB>
		inline bool operator ()(const AABB &aabb) const;
	};

	template<Axis axis>
	class AABBAxisBounds
	{
		float &min, &max;

	public:
		AABBAxisBounds(float &min, float &max) : min(min), max(max) {}

	public:
		template<class AABB>
		inline void operator ()(const AABB &aabb) const;
	};

	template<class AABBHandler>
	struct Object2AABB : public AABBHandler
	{
		using AABBHandler::AABBHandler;

		template<class Object>
		inline auto operator ()(const Object &object) const;
	};

	// currently for static geometry only
	template<class Object, TreeStructure treeStructure>
	class BVH
	{
		std::vector<Object> objects;

	public:
		class Node
		{
			decltype(std::declval<Object>().GetAABB()) aabb;
			std::unique_ptr<Node> children[treeStructure];
			unsigned char childrenOrder[treeStructure];
			unsigned int childrenCount;
			typename std::enable_if_t<true, decltype(objects)>::const_iterator objBegin, objEnd;
			WRL::ComPtr<ID3D12CommandList> bundle;
			bool visible;
			mutable bool cullExlusiveObjects;

		private:
			template<std::remove_extent_t<decltype(childrenOrder)> ...idx>
			Node(std::integer_sequence<std::remove_extent_t<decltype(childrenOrder)>, idx...>);

		public:
			Node(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, ...);
			Node(Node &&) = default;
			Node &operator =(Node &&) = default;

		private:
			void CreateChildNode(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, double overlapThreshold, unsigned int idxOffset);
			template<Axis axis, class F>
			void Split2(const F &action, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset = 0);
			void SplitQuadtree(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset = 0);
			void SplitOctree(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold);
			template<Axis axis, class F>
			void Split3(const F &action, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, unsigned int idxOffset = 0);
			void SplitEneaTree(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, unsigned int idxOffset = 0);
			void SplitIcoseptree(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint);

		public:
			const auto &GetAABB() const noexcept { return aabb; }
			auto GetObjectsRange() const noexcept { return std::make_pair(objBegin, objEnd); }
			unsigned long int GetExclusiveTriCount() const noexcept, GetInclusiveTriCount() const noexcept;
			float GetOcclusion() const noexcept;	// exclusive
			bool CullExclusiveObjects() const noexcept { return cullExlusiveObjects; }

		public:
			void TraverseOrdered(const std::function<void (Node &node)> &nodeHandler);
			void TraverseParallel(const std::function<void (Node &node)> &nodeHandler, const std::optional<const FrustumCuller<std::enable_if_t<true, decltype(aabb.Center())>::dimension>> &frustumCuller, const HLSL::float4x3 *depthSortXform, bool parentFullyVisible = false);
			float CollectOcclusionQueryBoxes(const Node **boxesBegin, const Node **boxesEnd) const;
		};

	private:
		std::unique_ptr<Node> root;

	public:
		BVH(SplitTechnique splitTechnique, ...);	// overlapThreshold -> ... for QUADTREE/OCTREE
		BVH(BVH &&) = default;
		BVH &operator =(BVH &&) = default;

	public:
		void TraverseOrdered(const std::function<void(Node &node)> &nodeHandler);
		void TraverseParallel(const std::function<void (Node &node)> &nodeHandler, const HLSL::float4x4 *frustumXform = nullptr/*nullptr -> use cached visibility*/, const HLSL::float4x3 *depthSortXform = nullptr);
	};
}