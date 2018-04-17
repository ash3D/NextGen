/**
\author		Alexey Shaydurov aka ASH
\date		17.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "AABB_3D.hlsli"
#include "per-frame data.hlsli"

struct AABB
{
	float3 min : AABB_min, max : AABB_max;
};

inline float4 XformDir(uint axis, float extent)
{
	const float3 worldSpaceDir = extent * terrainWorldXform[axis];
	const float3 viewSpaceDir = mul(worldSpaceDir, (float3x3)viewXform);
	return mul(viewSpaceDir, (float3x4)projXform);
}

#define ROOT_SIGNATURE														\
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | ALLOW_STREAM_OUTPUT),"	\
	"CBV(b0, visibility = SHADER_VISIBILITY_VERTEX)"

[RootSignature(ROOT_SIGNATURE)]
ClipSpaceAABB main(in uint quadVertexID : SV_VertexID, in AABB aabb)
{
	const float3 worldPos = mul(float4(aabb.min, 1.f), terrainWorldXform);
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	const float3 extents = aabb.max - aabb.min;
	const ClipSpaceAABB xformedAABB = { mul(float4(viewPos, 1.f), projXform), XformDir(0, extents[0]), XformDir(1, extents[1]), XformDir(2, extents[2]) };
	return xformedAABB;
}