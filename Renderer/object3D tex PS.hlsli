#ifndef OBJECT3D_TEX_PS_INCLUDED
#define OBJECT3D_TEX_PS_INCLUDED

#define ENABBLE_TEX

enum TextureID
{
	ALBEDO_MAP,
	GLASS_MASK,
};

#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D material.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "samplers.hlsli"
#include "glass.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<TonemapParams> tonemapParams : register(b1, space1);

float4 PS(in XformedVertex_UV input, in bool front, uniform bool enableGlassMask)
{
	input.N = normalize(front ? +input.N : -input.N);	// handles two-sided materials
	input.viewDir = normalize(input.viewDir);
	FixNormal(input.N, input.viewDir);

	const float3 albedo = SelectTexture(ALBEDO_MAP).Sample(obj3DAlbedoSampler, input.uv);
	float roughness = .5f, f0 = F0(1.55f);
	if (enableGlassMask)
		ApplyGlassMask(input.uv, roughness, f0);

	const float3 color = Lit(albedo, roughness, f0, input.N, input.viewDir, sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}

#endif	// OBJECT3D_TEX_PS_INCLUDED