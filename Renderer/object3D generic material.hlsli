#pragma once

#include "object3D materials common.hlsli"
#include "object3D tex stuff.hlsli"
#include "fresnel.hlsli"

namespace Materials
{
	struct Attribs
	{
		float3 albedo;
		float roughness, f0;
	};

	Attribs EvaluateGenericMaterial(float2 uv)
	{
		const Attribs result =
		{
			SelectTexture(ALBEDO_MAP).Sample(SelectSampler(TextureSamplers::OBJ3D_ALBEDO_SAMPLER), uv).rgb,
			.5f, Fresnel::F0(1.55f)
		};

		return result;
	}
}