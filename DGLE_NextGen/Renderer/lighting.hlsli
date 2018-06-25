/**
\author		Alexey Shaydurov aka ASH
\date		25.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#ifndef LIGHTING_INCLUDED
#define LIGHTING_INCLUDED

#include "fresnel.hlsli"

// evaluate Smith ^() function for GGX NDF
float GGXSmithIntegral(float a2, float NdotDir)
{
	const float cos2 = NdotDir * NdotDir;
	return .5f * sqrt((a2 - a2 * cos2) / cos2 + 1) - .5f;
}

// height-direction-correlated GGX Smith masking & shadowing (from http://jcgt.org/published/0003/02/03/paper.pdf)
const float G(float a2, float VdotN, float LdotN, float VdotL)
{
	const float phi = acos(VdotL);
	float lambda = 4.41 * phi;
	lambda /= lambda + 1;
	const float LambdaV = GGXSmithIntegral(a2, VdotN), LambdaL = GGXSmithIntegral(a2, LdotN);
	return rcp(1 + max(LambdaV, LambdaL) + lambda * min(LambdaV, LambdaL));
}

// evaluate rendering equation for punctual light source, non-metal material
// no low-level optimizations yet (such as avoiding H calculation in https://twvideo01.ubm-us.net/o1/vault/gdc2017/Presentations/Hammon_Earl_PBR_Diffuse_Lighting.pdf)
float3 Lit(float3 albedo, float roughness, float F0, float N, float3 viewDir, float3 lightDir, float3 lightIrradiance)
{
	// TODO: move outside
	static const float PI_rcp = .318309886183790671538f;

	const float VdotN = dot(viewDir, N);

	/*
		Handles both two-sided materials and vertex normal interpolation / normal mapping.
		In the latter case backface ideally should be never visible and advanced techniques such as POM can ensure it
			by picking N at view ray intersection point.
		Here just replace N with some frontfacing so that it won't appear dark (with proper light direction).
	*/
	N *= sign(VdotN);

	const float LdotN = dot(lightDir, N);

	// early out (backlighting)
	[branch]
	if (LdotN <= 0)
		return 0;

	const float a2 = roughness * roughness, VdotL = dot(viewDir, lightDir);
	const float3 H = normalize(viewDir + lightDir);
	const float NdotH = dot(N, H);

	float GGX_denom = NdotH * NdotH * (a2 - 1) + 1;	// without PI term
	GGX_denom *= GGX_denom;

	// optimization opportunity: merge NDF and G denoms
	const float spec = FresnelShlick(F0, dot(lightDir, H)) * G(a2, VdotN, LdotN, VdotL) * (.25f * PI_rcp) * a2 / (GGX_denom * LdotN);

	// GGX diffuse approximation from https://twvideo01.ubm-us.net/o1/vault/gdc2017/Presentations/Hammon_Earl_PBR_Diffuse_Lighting.pdf\
	!: potential mad optimizations via manual '(1 - x) * y' transformations
	const float
		facing = .5f * VdotL + .5f,
		rough = facing * (.9f - .4f * facing) * (.5f * NdotH + 1),
		smooth = (1 - FresnelShlick(F0, LdotN)) * 1.05f * (1 - pow(1 - abs(VdotN), 5)),
		single = PI_rcp * lerp(smooth, rough, roughness),
		multi = .1159f * roughness;

	const float3 diffuse = albedo * (single + albedo * multi);

	return (spec + diffuse) * lightIrradiance;
}

#endif	// LIGHTING_INCLUDED