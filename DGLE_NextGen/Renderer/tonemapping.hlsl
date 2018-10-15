/**
\author		Alexey Shaydurov aka ASH
\date		15.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "tonemapping config.hlsli"
#include "luminance.hlsli"

Texture2D src : register(t0);
RWTexture2D<float4> dst : register(u0);
cbuffer TonemapParams : register(b0)
{
	float exposure, whitePointFactor/*1/whitePoint^2*/;
};

float3 Reinhard(float3 color, float whitePointFactor)
{
	const float L = RGB_2_luminance(color);
	return color * ((1 + L * whitePointFactor) / (1 + L));
}

[numthreads(blockSize, blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	float4 srcPixel = src[coord];
	srcPixel.rgb *= exposure / srcPixel.a;
	dst[coord] = float4(Reinhard(srcPixel.rgb, whitePointFactor), 1);
}