#pragma once

// TODO: move to include or General

#include <cmath>
#include <type_traits>
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif
#include "SIMD.h"

namespace Renderer
{
	namespace HLSL = Math::VectorMath::HLSL;

	template<unsigned dimension>
	struct AABB
	{
		static_assert(dimension == 2 || dimension == 3, "AABB dimension can be either 2 or 3");
		Math::VectorMath::vector<float, dimension> min, max;

	public:
		inline AABB() : min(+INFINITY), max(-INFINITY) {}
		inline AABB(const Math::VectorMath::vector<float, dimension> &min, const Math::VectorMath::vector<float, dimension> &max) : min(min), max(max) {}

	private:
		void Refit(const Math::VectorMath::vector<float, dimension> &min, const Math::VectorMath::vector<float, dimension> &max);

	public:
		void Refit(const Math::VectorMath::vector<float, dimension> &vert), Refit(const AABB &src);
		void Transform(const Math::VectorMath::matrix<float, dimension + 1, dimension> &xform);
		inline Math::VectorMath::vector<float, dimension> Center() const;
		inline auto Size() const { return max - min; }
		inline float Measure() const;	// square for 2D, volume for 3D
	};

	template<unsigned dimension>
	struct ClipSpaceAABB
	{
		static_assert(dimension == 2 || dimension == 3, "AABB dimension can be either 2 or 3");
		Math::VectorMath::vector<std::conditional_t<dimension == 2, Math::SIMD::XMM, Math::SIMD::YMM>, 4> verts;

	public:
		explicit ClipSpaceAABB(const HLSL::float4x4 &xform, const AABB<dimension> &aabb);

	public:
		explicit operator AABB<2>() const;
		explicit operator AABB<3>() const;

	public:
		float MinW() const;
	};

	template<unsigned dimension>
	inline auto TransformAABB(AABB<dimension> aabb, const Math::VectorMath::matrix<float, dimension + 1, dimension> &xform)
	{
		aabb.Transform(xform);
		return aabb;
	}

	inline auto TransformAABB(const AABB<2> &aabb, const HLSL::float4x3 &xform)
	{
		AABB<3> aabb3D(HLSL::float3(aabb.min, 0.f), HLSL::float3(aabb.max, 0.f));
		aabb3D.Transform(xform);
		return aabb3D;
	}
}

#include "../AABB.inl"