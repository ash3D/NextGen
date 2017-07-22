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
	row_major float4x3 viewXform, worldXform;
};

float4 main(in float4 pos : POSITION ) : SV_POSITION
{
	const float3 worldPos = mul(pos, worldXform);
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	return mul(float4(viewPos, 1.f), projXform);
}