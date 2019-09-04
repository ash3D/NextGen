#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"
#include "terrain samplers.hlsli"

ConstantBuffer<Tonemapping::TonemapParams> tonemapParams : register(b0, space1);

cbuffer Constants : register(b1, space1)
{
	float3 albedo;
}

Texture2D tex : register(t0);

float4 main(in float3 viewDir : ViewDir, in float2 uv : UV) : SV_TARGET
{
	// same note on normal as for Flat shader
	const float3 color = Lighting::Lit(albedo * tex.Sample(TerrainSamplers::albedo, uv), .5f, Fresnel::F0(1.55f), mul(-terrainWorldXform[2], viewXform), normalize(viewDir), sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}