/**
\author		Alexey Shaydurov aka ASH
\date		07.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "per-frame data.hlsli"

cbuffer InstanceData : register(b1)
{
	row_major float4x3 worldXform;
};

float4 main(in float4 pos : POSITION, out float height : SV_ClipDistance) : SV_POSITION
{
	const float3 worldPos = mul(float4(mul(pos, worldXform), 1.f), terrainWorldXform);
	height = worldPos.z;	// do not render anything under terrain
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	return mul(float4(viewPos, 1.f), projXform);
}