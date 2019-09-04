#pragma once

#define ENABBLE_TEX

namespace Materials
{
	enum TextureID
	{
		ALBEDO_MAP,
		NORMAL_MAP,
		GLASS_MASK,
	};
}

#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D material.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "glass.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<Tonemapping::TonemapParams> tonemapParams : register(b1, space1);

// preserve 0
float3 SafeNormalize(float3 vec)
{
	vec = normalize(vec);
	return isfinite(vec) ? vec : 0;
}

float4 BumpPS(in XformedVertex_UV_TG input, in bool front, uniform bool enableGlassMask)
{
	//using namespace Lighting;
	//using namespace Materials;

	input.viewDir = normalize(input.viewDir);
#if 0
	// validation error on current HLSL compiler, try with future version
	float3x3 TBN = { SafeNormalize(input.tangents[0]), SafeNormalize(input.tangents[1]), normalize(input.N) };
	if (!front)	// handle two-sided materials
		TBN = -TBN;
#else
	float3x3 TBN = { SafeNormalize(front ? +input.tangents[0] : -input.tangents[0]), SafeNormalize(front ? +input.tangents[1] : -input.tangents[1]), normalize(front ? +input.N : -input.N) };	// handles two-sided materials
#endif
	Lighting::FixTBN(TBN, input.viewDir);
	float3 n;
	n.xy = SelectTexture(Materials::NORMAL_MAP).Sample(SelectSampler(TextureSamplers::OBJ3D_BUMP_SAMPLER), input.uv) * input.normalScale;
	n.z = sqrt(saturate(1.f - length(n.xy)));
	n = mul(n, TBN);

	const float3 albedo = SelectTexture(Materials::ALBEDO_MAP).Sample(SelectSampler(TextureSamplers::OBJ3D_ALBEDO_SAMPLER), input.uv);
	float roughness = .5f, f0 = Fresnel::F0(1.55f);
	if (enableGlassMask)
		Materials::ApplyGlassMask(input.uv, roughness, f0);

	const float3 color = Lighting::Lit(albedo, roughness, f0, TBN[2], n, input.viewDir, sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}