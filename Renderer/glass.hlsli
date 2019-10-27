#pragma once

#include "object3D tex stuff.hlsli"
#include "fresnel.hlsli"

namespace Materials
{
	void ApplyGlassMask(in float2 uv, inout float roughness, inout float f0)
	{
		static const float threshold = .8f, glassRough = 1e-2f, glassIOR = 1.51714f;

		const float glass = SelectTexture(GLASS_MASK).Sample(SelectSampler(TextureSamplers::OBJ3D_GLASS_MASK_SAMPLER), uv);
		if (glass >= threshold)
		{
			// just replace rough & IOR for now, consider multi-layered material instead
			roughness = glassRough;
			f0 = Fresnel::F0(glassIOR);
		}
		else
		{
			// not actually glass, interpret glass mask other way: bump up rough & IOR in some manner
			roughness *= 1 - glass;
			f0 = lerp(f0, 1, glass);
		}
	}
}