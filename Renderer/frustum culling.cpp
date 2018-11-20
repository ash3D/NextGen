/**
\author		Alexey Shaydurov aka ASH
\date		23.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "frustum culling.h"

using namespace Renderer::Impl;
using namespace HLSL;
using Math::VectorMath::vector;

// TODO: implment transpose in vector math
static inline float4 Column(const float4x4 &m, unsigned c)
{
	return { m[0][c], m[1][c], m[2][c], m[3][c] };
}

template<unsigned int dimension>
inline vector<float, dimension + 1> FrustumCuller<dimension>::ExtractUsedCoords(const float4 &src)
{
	return { vector<float, dimension>(src), src.w };
}

template<unsigned int dimension>
FrustumCuller<dimension>::FrustumCuller(const float4x4 &frustumXform) :
	frustumPlanes
	{
		-ExtractUsedCoords(Column(frustumXform, 0)) - ExtractUsedCoords(Column(frustumXform, 3)),	// -X
		+ExtractUsedCoords(Column(frustumXform, 0)) - ExtractUsedCoords(Column(frustumXform, 3)),	// +X
		-ExtractUsedCoords(Column(frustumXform, 1)) - ExtractUsedCoords(Column(frustumXform, 3)),	// -Y
		+ExtractUsedCoords(Column(frustumXform, 1)) - ExtractUsedCoords(Column(frustumXform, 3)),	// +Y
		-ExtractUsedCoords(Column(frustumXform, 3))													// -Z
		+ExtractUsedCoords(Column(frustumXform, 2)) - ExtractUsedCoords(Column(frustumXform, 3))	// +Z
	},
	// TODO: implement abs in vector math (with specialization for floating point types using fabs)
	absFrustumPlanes
	{
		frustumPlanes[0].apply(fabsf),
		frustumPlanes[1].apply(fabsf),
		frustumPlanes[2].apply(fabsf),
		frustumPlanes[3].apply(fabsf),
		frustumPlanes[4].apply(fabsf),
		frustumPlanes[5].apply(fabsf)
	}
{}

template class FrustumCuller<2>;
template class FrustumCuller<3>;