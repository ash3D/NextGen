/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

cbuffer Xforms : register(b0)
{
	row_major float4x4 projXform;
	row_major float4x3 viewXform;
};

static const float size = 150000.f;
static const float2 center = { -12000.f, -10000.f }, quad[] =
{
	float2(-size, -size), float2(-size, +size),
	float2(+size, -size), float2(+size, +size)
};

float4 main(/*float2 pos : POSITION*/uint idx : SV_VertexID, out float2 uv : TEXCOORD) : SV_POSITION
{
	const float2 pos = quad[idx] + center;
	const float3 viewPos = mul(float4(pos, 0.f, 1.f), viewXform);
	uv = pos;
	return mul(float4(viewPos, 1.f), projXform);
}