/**
\author		Alexey Shaydurov aka ASH
\date		27.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "per-frame data.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "lighting.hlsli"

cbuffer MaterialConstants : register(b0, space1)
{
	float3 albedo;
}

float4 main(in XformedVertex input) : SV_TARGET
{
	const float3 color = Lit(albedo, .5f, F0(1.55f), normalize(input.N), normalize(input.viewDir), sun.dir, sun.irradiance);

	// built-in simple tonemapping for now
	return float4(color / (color + 1), 1.f);
}