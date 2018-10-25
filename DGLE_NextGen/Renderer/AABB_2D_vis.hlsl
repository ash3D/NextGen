/**
\author		Alexey Shaydurov aka ASH
\date		26.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "tonemap params.hlsli"

cbuffer Constants : register(b0, space1)
{
	float3 color;
}

ConstantBuffer<TonemapParams> tonemapParams : register(b1, space1);

float4 main() : SV_TARGET
{
	return float4(color, tonemapParams.exposure);
}