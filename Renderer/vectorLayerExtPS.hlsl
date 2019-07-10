#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"
#include "samplers.hlsli"

ConstantBuffer<TonemapParams> tonemapParams : register(b0, space1);

Texture2D albedoMap : register(t0), fresnelMap : register(t1), roughnessMap : register(t2), normalMap : register(t3);

float4 main(in float3 viewDir : ViewDir, in float2 uv : UV) : SV_TARGET
{
	// same notes on normal as for std material
	float3 n;
	n.xy = normalMap.Sample(terrainBumpSampler, uv);
	n.z = -sqrt(saturate(1.f - length(n.xy)));
	n = mul(mul(n, terrainWorldXform), viewXform);

	const float3 albedo = albedoMap.Sample(terrainAlbedoSampler, uv);
	const float roughness = roughnessMap.Sample(terrainRoughnessSampler, uv), fresnel = fresnelMap.Sample(terrainFresnelSampler, uv);
	const float3 color = Lit(albedo, roughness, fresnel, viewXform[2], n, normalize(viewDir), sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}