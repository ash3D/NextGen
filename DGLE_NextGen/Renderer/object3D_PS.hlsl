/**
\author		Alexey Shaydurov aka ASH
\date		26.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "lighting.hlsli"

cbuffer MaterialConstants : register(b0, space1)
{
	float3 albedo;
}

ConstantBuffer<TonemapParams> tonemapParams : register(b1, space1);

float4 main(in XformedVertex input) : SV_TARGET
{
	const float3 color = Lit(albedo, .5f, F0(1.55f), normalize(input.N), normalize(input.viewDir), sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}