/**
\author		Alexey Shaydurov aka ASH
\date		05.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "per-frame data.hlsli"
#include "lighting.hlsli"

cbuffer Constants : register(b0, space1)
{
	float3 albedo;
}

float4 main(in float3 viewDir : ViewDir) : SV_TARGET
{
	/*
		xform (0, 0, 1) normal by terrainWorldXform matrix, then by viewXform
		assumes both terrainWorldXform and viewXform are orthonormal, need inverse transpose otherwise
		current mul is suboptimal: viewXform[2] can be used if terrainWorldXform keeps Z line (which is the case) or merged in worldViewXform
	*/
	const float3 color = Lit(albedo, .5f, F0(1.55f), mul(terrainWorldXform[2], viewXform), normalize(viewDir), sun.dir, sun.irradiance);

	// built-in simple tonemapping for now
	return Reinhard(color);
}