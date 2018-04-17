/**
\author		Alexey Shaydurov aka ASH
\date		17.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "AABB_3D.hlsli"

// same considerations regarding 'VS + hardware instancing' vs 'box generation in GS' as for 2D AABB applicable here

float4 main(in uint boxVertexID : SV_VertexID, in ClipSpaceAABB aabb) : SV_POSITION
{
	return aabb.corner +
		aabb.extents[0] * (boxVertexID & 1u) +
		aabb.extents[1] * (boxVertexID >> 1u & 1u) +
		aabb.extents[2] * (boxVertexID >> 2u & 1u);
}