/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "AABB.h"
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif
#include "SIMD.h"
#include <immintrin.h>

namespace Renderer::Impl
{
	namespace HLSL = Math::VectorMath::HLSL;

	enum class CullResult
	{
		INSIDE,
		OUTSIDE,
		UNDETERMINED,	// maybe intersected or outside
	};

	inline CullResult FrustumCull(const ClipSpaceAABB<2> &aabb)
	{
		using Math::SIMD::XMM;

		int clipMask, clipMaskAccum = 0;

		// -X
		if ((clipMask = _mm_movemask_ps(_mm_cmplt_ps(static_cast<const XMM &>(aabb.verts.x), static_cast<const XMM &>(-aabb.verts.w)))) == 0xf)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// +X
		if ((clipMask = _mm_movemask_ps(_mm_cmpgt_ps(static_cast<const XMM &>(aabb.verts.x), static_cast<const XMM &>(+aabb.verts.w)))) == 0xf)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// -Y
		if ((clipMask = _mm_movemask_ps(_mm_cmplt_ps(static_cast<const XMM &>(aabb.verts.y), static_cast<const XMM &>(-aabb.verts.w)))) == 0xf)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// +Y
		if ((clipMask = _mm_movemask_ps(_mm_cmpgt_ps(static_cast<const XMM &>(aabb.verts.y), static_cast<const XMM &>(+aabb.verts.w)))) == 0xf)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// -Z
		if ((clipMask = _mm_movemask_ps(_mm_cmplt_ps(static_cast<const XMM &>(aabb.verts.z), XMM()))) == 0xf)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// +Z
		if ((clipMask = _mm_movemask_ps(_mm_cmpgt_ps(static_cast<const XMM &>(aabb.verts.z), static_cast<const XMM &>(+aabb.verts.w)))) == 0xf)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		return clipMaskAccum ? CullResult::UNDETERMINED : CullResult::INSIDE;
	}

	inline CullResult FrustumCull(const ClipSpaceAABB<3> &aabb)
	{
		using Math::SIMD::YMM;

		int clipMask, clipMaskAccum = 0;

		// -X
		if ((clipMask = _mm256_movemask_ps(_mm256_cmp_ps(static_cast<const YMM &>(aabb.verts.x), static_cast<const YMM &>(-aabb.verts.w), _CMP_LT_OQ))) == 0xff)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// +X
		if ((clipMask = _mm256_movemask_ps(_mm256_cmp_ps(static_cast<const YMM &>(aabb.verts.x), static_cast<const YMM &>(+aabb.verts.w), _CMP_GT_OQ))) == 0xff)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// -Y
		if ((clipMask = _mm256_movemask_ps(_mm256_cmp_ps(static_cast<const YMM &>(aabb.verts.y), static_cast<const YMM &>(-aabb.verts.w), _CMP_LT_OQ))) == 0xff)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// +Y
		if ((clipMask = _mm256_movemask_ps(_mm256_cmp_ps(static_cast<const YMM &>(aabb.verts.y), static_cast<const YMM &>(+aabb.verts.w), _CMP_GT_OQ))) == 0xff)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// -Z
		if ((clipMask = _mm256_movemask_ps(_mm256_cmp_ps(static_cast<const YMM &>(aabb.verts.z), YMM(), _CMP_LT_OQ))) == 0xff)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		// +Z
		if ((clipMask = _mm256_movemask_ps(_mm256_cmp_ps(static_cast<const YMM &>(aabb.verts.z), static_cast<const YMM &>(+aabb.verts.w), _CMP_GT_OQ))) == 0xff)
			return CullResult::OUTSIDE;
		clipMaskAccum |= clipMask;

		return clipMaskAccum ? CullResult::UNDETERMINED : CullResult::INSIDE;
	}

	template<unsigned int dimension>
	class FrustumCuller
	{
		Math::VectorMath::vector<float, dimension + 1> frustumPlanes[6];
		Math::VectorMath::vector<float, dimension> absFrustumPlanes[6];

	public:
		FrustumCuller(const HLSL::float4x4 &frustumXform);

	public:
		// with earlyOut returns true if culled
		template<bool earlyOut>
		std::conditional_t<earlyOut, bool, CullResult> Cull(const AABB<dimension> &aabb) const;

	private:
		static Math::VectorMath::vector<float, dimension + 1> ExtractUsedCoords(const HLSL::float4 &src);
	};

	// TODO: use C++17 constexpr if billets when it will be supported
	template<unsigned int dimension>
	template<bool earlyOut>
	inline std::conditional_t<earlyOut, bool, CullResult> FrustumCuller<dimension>::Cull(const AABB<dimension> &aabb) const
	{
		// temp for MSVC
		typedef std::conditional_t<earlyOut, bool, CullResult> Reult;

		const auto aabbCenter = aabb.Center();
		const auto aaabbExtent = aabb.Size() * .5f;

		const float leftCenterDist = dot(frustumPlanes[0], aabbCenter) + frustumPlanes[0][dimension];
		const float leftCornerOffset = dot(absFrustumPlanes[0], aaabbExtent);
		if (leftCenterDist - leftCornerOffset > 0.f)
		{
			if /*constexpr*/ (earlyOut)
				return (Reult)true;
			else
				return (Reult)CullResult::OUTSIDE;
		}

		const float rightCenterDist = dot(frustumPlanes[1], aabbCenter) + frustumPlanes[1][dimension];
		const float rightCornerOffset = dot(absFrustumPlanes[1], aaabbExtent);
		if (rightCenterDist - rightCornerOffset > 0.f)
		{
			if /*constexpr*/ (earlyOut)
				return (Reult)true;
			else
				return (Reult)CullResult::OUTSIDE;
		}

		const float bottomCenterDist = dot(frustumPlanes[2], aabbCenter) + frustumPlanes[2][dimension];
		const float bottomCornerOffset = dot(absFrustumPlanes[2], aaabbExtent);
		if (bottomCenterDist - bottomCornerOffset > 0.f)
		{
			if /*constexpr*/ (earlyOut)
				return (Reult)true;
			else
				return (Reult)CullResult::OUTSIDE;
		}

		const float topCenterDist = dot(frustumPlanes[3], aabbCenter) + frustumPlanes[3][dimension];
		const float topCornerOffset = dot(absFrustumPlanes[3], aaabbExtent);
		if (topCenterDist - topCornerOffset > 0.f)
		{
			if /*constexpr*/ (earlyOut)
				return (Reult)true;
			else
				return (Reult)CullResult::OUTSIDE;
		}

		const float nearCenterDist = dot(frustumPlanes[4], aabbCenter) + frustumPlanes[4][dimension];
		const float nearCornerOffset = dot(absFrustumPlanes[4], aaabbExtent);
		if (nearCenterDist - nearCornerOffset > 0.f)
		{
			if /*constexpr*/ (earlyOut)
				return (Reult)true;
			else
				return (Reult)CullResult::OUTSIDE;
		}

		const float farCenterDist = dot(frustumPlanes[5], aabbCenter) + frustumPlanes[5][dimension];
		const float farCornerOffset = dot(absFrustumPlanes[5], aaabbExtent);
		if (farCenterDist - farCornerOffset > 0.f)
		{
			if /*constexpr*/ (earlyOut)
				return (Reult)true;
			else
				return (Reult)CullResult::OUTSIDE;
		}

		if /*constexpr*/ (earlyOut)
			return (Reult)false;
		else
		{
			const bool inside =
				leftCenterDist + leftCornerOffset <= 0.f &&
				rightCenterDist + rightCornerOffset <= 0.f &&
				bottomCenterDist + bottomCornerOffset <= 0.f &&
				topCenterDist + topCornerOffset <= 0.f &&
				nearCenterDist + nearCornerOffset <= 0.f &&
				farCenterDist + farCornerOffset <= 0.f;
			return Reult(inside ? CullResult::INSIDE : CullResult::UNDETERMINED);
		}
	}
}