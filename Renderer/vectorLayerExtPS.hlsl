#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"
#include "terrain samplers.hlsli"

ConstantBuffer<TonemapParams> tonemapParams : register(b0, space1);

Texture2D albedoMap : register(t0), fresnelMap : register(t1), roughnessMap : register(t2), normalMap : register(t3);

float4 main(in float3 viewDir : ViewDir, in float2 uv : UV) : SV_TARGET
{
	// same notes on normal as for std material
	float3 n;
	n.xy = normalMap.Sample(TerrainSamplers::bump, uv);
	n.z = -sqrt(saturate(1.f - length(n.xy)));
	n = mul(mul(n, terrainWorldXform), viewXform);

	const float3 albedo = albedoMap.Sample(TerrainSamplers::albedo, uv);
	const float roughness = roughnessMap.Sample(TerrainSamplers::roughness, uv), fresnel = fresnelMap.Sample(TerrainSamplers::fresnel, uv);
	const float3 color = Lit(albedo, roughness, fresnel, viewXform[2], n, normalize(viewDir), sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}