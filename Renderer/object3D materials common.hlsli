#pragma once

#include "object3D common samplers.hlsli"

#define DXC_NAMESPACE_WORKAROUND 1

#if DXC_NAMESPACE_WORKAROUND && defined ENABBLE_TEX
Texture2D textures[] : register(t0);
#endif

namespace Materials
{
#ifdef ENABBLE_TEX
	SamplerState TV_sampler : register(s0), samplers[TextureSamplers::OBJ3D_COMMON_SAMPLERS_COUNT * 2] : register(s1);
	Texture2D textures[] : register(t0);
#endif

	cbuffer MaterialConstants : register(b0, space1)
	{
		float3	albedo;
		uint	descriptorTablesOffsets;	// packed [31..16] - sampler, [15..0] - tex
		float	TVBrighntess;

#ifdef ENABBLE_TEX
		// TextureID is an enum to be defined before this file #inclusion
		Texture2D SelectTexture(TextureID id)
		{
			return textures[(descriptorTablesOffsets & 0xFFFF) + id];
		}

		SamplerState SelectSampler(TextureSamplers::Object3DCommonSamplerID id)
		{
			return samplers[(descriptorTablesOffsets >> 16) + id];
		}
#endif
	}
}

#undef DXC_NAMESPACE_WORKAROUND