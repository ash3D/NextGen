#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "lighting.hlsli"
#include "samplers.hlsli"

ConstantBuffer<TonemapParams> tonemapParams : register(b0, space1);

Texture2D albedoMap : register(t0), fresnelMap : register(t1), roughnessMap : register(t2), normalMap : register(t3);

float4 main(in float3 viewDir : ViewDir, in float2 uv : UV) : SV_TARGET
{
	// same notes on normal as for std material
	float3 N;
	N.xy = normalMap.Sample(terrainBumpSampler, uv);
	N.z = sqrt(saturate(1.f - length(N.xy)));
	N = mul(N, viewXform);

	const float3 albedo = albedoMap.Sample(terrainAlbedoSampler, uv);
	const float roughnes = roughnessMap.Sample(terrainRoughnessSampler, uv), fresnel = fresnelMap.Sample(terrainFresnelSampler, uv);
	const float3 color = Lit(albedo, roughnes, fresnel, N, normalize(viewDir), sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}