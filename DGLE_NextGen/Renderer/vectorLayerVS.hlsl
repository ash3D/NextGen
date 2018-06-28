/**
\author		Alexey Shaydurov aka ASH
\date		29.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "per-frame data.hlsli"

struct XformedVertex
{
	float3 viewDir		: ViewDir;
	float4 xformedPos	: SV_POSITION;
};

XformedVertex main(in float4 pos : POSITION)
{
	const float3 worldPos = mul(pos, terrainWorldXform);
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	const XformedVertex output = { -viewPos, mul(float4(viewPos, 1.f), projXform) };
	return output;
}