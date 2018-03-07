/**
\author		Alexey Shaydurov aka ASH
\date		07.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "AABB.h"
#include <algorithm>

template<unsigned dimension>
inline void Renderer::AABB<dimension>::Refit(const Math::VectorMath::vector<float, dimension> &min, const Math::VectorMath::vector<float, dimension> &max)
{
	// TODO: specialize vector math min/max for FP types to use fmin/fmax internally
	for (unsigned i = 0; i < dimension; i++)
	{
		this->min[i] = fmin(this->min[i], min[i]);
		this->max[i] = fmax(this->max[i], max[i]);
	}
}

template<unsigned dimension>
inline void Renderer::AABB<dimension>::Refit(const Math::VectorMath::vector<float, dimension> &vert)
{
	Refit(vert, vert);
}

template<unsigned dimension>
inline void Renderer::AABB<dimension>::Refit(const AABB &src)
{
	Refit(src.min, src.max);
}

template<unsigned dimension>
void Renderer::AABB<dimension>::Transform(const Math::VectorMath::matrix<float, dimension + 1, dimension> &xform)
{
	using Math::VectorMath::vector;

	const auto size = Size();
	min = max = mul(vector<float, dimension + 1>(min, 1.f), xform);
	for (unsigned axisIdx = 0; axisIdx < dimension; axisIdx++)
	{
		const auto xformedAxis = size[axisIdx] * xform[axisIdx];
		for (unsigned coordIdx = 0; coordIdx < dimension; coordIdx++)
		{
			auto &target = xformedAxis[coordIdx] > 0.f ? max : min;
			target += xformedAxis[coordIdx];
		}
	}
}

template<>
inline float Renderer::AABB<2>::Measure() const
{
	const auto size = Size();
	return size.x * size.y;
}

template<>
inline float Renderer::AABB<3>::Measure() const
{
	const auto size = Size();
	return size.x * size.y * size.z;
}

inline Renderer::ClipSpaceAABB<2>::ClipSpaceAABB(const HLSL::float4x4 &xform, const AABB<2> &aabb)
{
	using namespace HLSL;

	const float2 size = aabb.Size();
	const float4 clipSpaceExtents[2] = { size[0] * xform[0], size[1] * xform[1] };
	static const Math::SIMD::XMM quadOffsets[2] = { { 0.f, 1.f, 0.f, 1.f }, { 0.f, 0.f, 1.f, 1.f } };
	verts = mul(float4(aabb.min, 0.f, 1.f), xform) + clipSpaceExtents[0] * quadOffsets[0] + clipSpaceExtents[1] * quadOffsets[1];
}

inline Renderer::ClipSpaceAABB<2>::operator Renderer::AABB<2>() const
{
	using Math::SIMD::XMM;
	using std::minmax;

	const Math::VectorMath::vector<XMM, 2> NDCQuadVerts(verts.xy / verts.w);
	const auto X = static_cast<const XMM &>(NDCQuadVerts.x).Extract(), Y = static_cast<const XMM &>(NDCQuadVerts.y).Extract();
	const auto XBounds = minmax({ X[0], X[1], X[2], X[3] }), YBounds = minmax({ Y[0], Y[1], Y[2], Y[3] });
	return { { XBounds.first, YBounds.first }, { XBounds.second, YBounds.second } };
}

inline Renderer::ClipSpaceAABB<2>::operator Renderer::AABB<3>() const
{
	const auto Z = static_cast<const Math::SIMD::XMM &>(verts.z / verts.w).Extract();
	const auto ZBounds = std::minmax({ Z[0], Z[1], Z[2], Z[3] });
	const AABB<2> aabb2D = operator AABB<2>();
	return { { aabb2D.min, ZBounds.first }, { aabb2D.max, ZBounds.second } };
}

inline float Renderer::ClipSpaceAABB<2>::MinW() const
{
	const auto W = static_cast<const Math::SIMD::XMM &>(verts.w).Extract();
	return std::min({ W[0], W[1], W[2], W[3] });
}

inline Renderer::ClipSpaceAABB<3>::ClipSpaceAABB(const HLSL::float4x4 &xform, const AABB<3> &aabb)
{
	using namespace HLSL;

	const float3 size = aabb.Size();
	const float4 clipSpaceExtents[3] = { size[0] * xform[0], size[1] * xform[1], size[2] * xform[2] };
	static const Math::SIMD::YMM boxOffsets[3] = { { 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f }, { 0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 1.f, 1.f }, { 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f } };
	verts = mul(float4(aabb.min, 1.f), xform) + clipSpaceExtents[0] * boxOffsets[0] + clipSpaceExtents[1] * boxOffsets[1] + clipSpaceExtents[2] * boxOffsets[2];
}

inline Renderer::ClipSpaceAABB<3>::operator Renderer::AABB<2>() const
{
	using Math::SIMD::YMM;
	using std::minmax;

	const Math::VectorMath::vector<YMM, 2> NDCBoxVerts(verts.xy / verts.w);
	const auto X = static_cast<const YMM &>(NDCBoxVerts.x).Extract(), Y = static_cast<const YMM &>(NDCBoxVerts.y).Extract();
	const auto XBounds = minmax({ X[0], X[1], X[2], X[3], X[4], X[5], X[6], X[7] }), YBounds = minmax({ Y[0], Y[1], Y[2], Y[3], Y[4], Y[5], Y[6], Y[7] });
	return { { XBounds.first, YBounds.first }, { XBounds.second, YBounds.second } };
}

inline Renderer::ClipSpaceAABB<3>::operator Renderer::AABB<3>() const
{
	const auto Z = static_cast<const Math::SIMD::YMM &>(verts.z / verts.w).Extract();
	const auto ZBounds = std::minmax({ Z[0], Z[1], Z[2], Z[3], Z[4], Z[5], Z[6], Z[7] });
	const AABB<2> aabb2D = operator AABB<2>();
	return { { aabb2D.min, ZBounds.first }, { aabb2D.max, ZBounds.second } };
}

inline float Renderer::ClipSpaceAABB<3>::MinW() const
{
	const auto W = static_cast<const Math::SIMD::YMM &>(verts.w).Extract();
	return std::min({ W[0], W[1], W[2], W[3], W[4], W[5], W[6], W[7] });
}