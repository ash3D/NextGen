/**
\author		Alexey Shaydurov aka ASH
\date		27.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "per-frame data.hlsli"
#include "object3D VS 2 PS.hlsli"

cbuffer InstanceData : register(b1)
{
	row_major float4x3 worldXform;
};

struct SrcVertex
{
	float4 pos	: POSITION;
	float3 N	: NORMAL;
};

XformedVertex main(in SrcVertex input, out float4 xformedPos : SV_POSITION, out float height : SV_ClipDistance)
{
	const float3 worldPos = mul(float4(mul(input.pos, worldXform), 1.f), terrainWorldXform);
	height = worldPos.z;	// do not render anything under terrain
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	xformedPos = mul(float4(viewPos, 1.f), projXform);

	/*
		xform normal
		!: use vector xform for now, need to replace with covector xform (inverse transpose) for correct non-uniform scaling handling
	*/
	const XformedVertex output = { -viewPos, mul(mul(mul(input.N, worldXform), terrainWorldXform), viewXform) };
	return output;
}