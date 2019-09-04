#pragma once

#define ENABBLE_TEX

namespace Materials
{
	enum TextureID
	{
		ALBEDO_MAP,
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

float4 TexPS(in XformedVertex_UV input, in bool front, uniform bool enableGlassMask)
{
	//using namespace Lighting;
	//using namespace Materials;

	input.N = normalize(front ? +input.N : -input.N);	// handles two-sided materials
	input.viewDir = normalize(input.viewDir);
	Lighting::FixNormal(input.N, input.viewDir);

	const float3 albedo = SelectTexture(Materials::ALBEDO_MAP).Sample(SelectSampler(TextureSamplers::OBJ3D_ALBEDO_SAMPLER), input.uv);
	float roughness = .5f, f0 = Fresnel::F0(1.55f);
	if (enableGlassMask)
		Materials::ApplyGlassMask(input.uv, roughness, f0);

	const float3 color = Lighting::Lit(albedo, roughness, f0, input.N, input.viewDir, sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}