#pragma once

#include "object3D materials common.hlsli"
#include "glass.hlsli"

namespace Materials
{
	struct Attribs
	{
		float3 albedo;
		float roughness, f0;
	};

	Attribs EvaluateGenericMaterial(in float2 uv, uniform bool enableGlassMask)
	{
		Attribs result =
		{
			SelectTexture(ALBEDO_MAP).Sample(SelectSampler(TextureSamplers::OBJ3D_ALBEDO_SAMPLER), uv).rgb,
			.5f, Fresnel::F0(1.55f)
		};

		if (enableGlassMask)
			ApplyGlassMask(uv, result.roughness, result.f0);

		return result;
	}
}