/**
\author		Alexey Shaydurov aka ASH
\date		05.11.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#define NOMINMAX

#include <utility>
#include <type_traits>
#include <memory>
#include <vector>
#include <wrl/client.h>
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif

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

	public:
		template<class Object>
		inline auto operator ()(const Object &object) const;
	};

	// currently for static geometry only
	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	class BVH
	{
		std::vector<Object> objects;

	public:
		class Node : public CustomNodeData
		{
			friend class BVH;

		private:
			std::decay_t<decltype(std::declval<Object>().GetAABB())> aabb;
			std::unique_ptr<Node> children[treeStructure];
			typename std::enable_if_t<true, decltype(objects)>::const_iterator objBegin, objExclusiveSeparator, objEnd;
			unsigned long int exclusiveTriCount, inclusiveTriCount;
			float occlusion;
			unsigned char childrenOrder[treeStructure];
			unsigned char childrenCount{};
			enum struct Visibility : unsigned char
			{
				Culled		= 0b10,	// completely culled by frustum
				Atomic		= 0b01,	// all children (and possibly node's exclusive objects) has the same visibility
				Composite	= 0b00,	// traverse for children required
			} visibility{};	// need to init since it can be accessed in CollectOcclusionQueryBoxes()
			enum struct OcclusionCullDomain : unsigned char
			{
				WholeNode		= 0b1111,
				ChildrenOnly	= 0b0111,
				ForceComposite	= 0b0010,
			} occlusionCullDomain{};	// can be overriden by parent during tree traverse; need to init in order to eliminate possible UB due to uninit read in OverrideOcclusionCullDomain()
			bool shceduleOcclusionQuery;

		private:
			template<std::remove_extent_t<decltype(childrenOrder)> ...idx>
			Node(std::integer_sequence<std::remove_extent_t<decltype(childrenOrder)>, idx...>, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end);

		public:	// for make_unique
			Node(typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, ...);
			Node(Node &&) = delete;
			Node &operator =(Node &&) = delete;

		private:
			template<typename ...Params>
			void CreateChildNode(bool splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, unsigned int idxOffset, Params ...params);
			template<Axis axis, class F>
			void Split2(const F &action, bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset = 0);
			void SplitQuadtree(bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset = 0);
			void SplitOctree(bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold);
			template<Axis axis, class F>
			void Split3(const F &action, bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, unsigned int idxOffset = 0);
			void SplitEneaTree(bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, unsigned int idxOffset = 0);
			void SplitIcoseptree(bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint);

		public:
			inline const auto &GetAABB() const noexcept { return aabb; }
			inline auto GetExclusiveObjectsRange() const noexcept { return std::make_pair(objBegin, objExclusiveSeparator); }
			inline auto GetInclusiveObjectsRange() const noexcept { return std::make_pair(objBegin, objEnd); }
			inline unsigned long int GetExclusiveTriCount() const noexcept { return exclusiveTriCount; }
			inline unsigned long int GetInclusiveTriCount() const noexcept { return inclusiveTriCount; }
			inline float GetOcclusion() const noexcept { return occlusion; }	// exclusive
			inline OcclusionCullDomain GetOcclusionCullDomain() const noexcept { return occlusionCullDomain; }
			inline bool OcclusionQueryShceduled() const noexcept { return shceduleOcclusionQuery; }
			Visibility GetVisibility(OcclusionCullDomain override) const noexcept;
			void OverrideOcclusionCullDomain(OcclusionCullDomain &domain) const noexcept;

		private:
			template<typename ...Args, typename F>
			void Traverse(F &nodeHandler, Args ...args);
			std::pair<unsigned long int, bool> Shcedule(const FrustumCuller<std::enable_if_t<true, decltype(aabb.Center())>::dimension> &frustumCuller, const HLSL::float4x4 &frustumXform, const HLSL::float4x3 *depthSortXform,
				bool parentInsideFrustum = false, float parentOcclusionCulledProjLength = INFINITY, float parentOcclusion = 0);
			std::pair<unsigned long int, float> CollectOcclusionQueryBoxes(const Node **boxesBegin, const Node **boxesEnd, Visibility parentVisibilityOverride = {});
		};

	private:
		std::unique_ptr<Node> root;

	public:
		template<typename Iterator>
		BVH(Iterator objBegin, Iterator objEnd, SplitTechnique splitTechnique, ...);	// overlapThreshold -> ... for QUADTREE/OCTREE
		BVH(BVH &&) = default;
		BVH &operator =(BVH &&) = default;

	public:
		template<typename ...Args, typename F>
		void Traverse(F &nodeHandler, const Args &...args);
		void Shcedule(const FrustumCuller<std::enable_if_t<true, decltype(std::declval<Object>().GetAABB().Center())>::dimension> &frustumCuller, const HLSL::float4x4 &frustumXform, const HLSL::float4x3 *depthSortXform = nullptr);
		void FreeObjects();
		unsigned long int GetTriCount() const { return root->GetInclusiveTriCount(); }
	};
}