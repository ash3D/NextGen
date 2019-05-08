#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "lighting.hlsli"
#include "samplers.hlsli"

ConstantBuffer<TonemapParams> tonemapParams : register(b0, space1);

cbuffer Material : register(b1, space1)
{
	float f0;
}

Texture2D albedoMap : register(t0), roughnessMap : register(t1), normalMap : register(t2);

float4 main(in float3 viewDir : ViewDir, in float2 uv : UV) : SV_TARGET
{
	/*
		TBN space for terrain in world space is
		{1, 0, 0}
		{0, 1, 0}
		{0, 0, 1}
	*/
	float3 n;
	n.xy = normalMap.Sample(terrainBumpSampler, uv);
	n.z = sqrt(saturate(1.f - length(n.xy)));

	/*
		xform normal 'world space' -> 'view space'
		similar note as for Flat shader applicable (viewXform assumed to be orthonormal, need inverse transpose / normalize otherwise)
	*/
	n = mul(n, viewXform);

	const float3 color = Lit(albedoMap.Sample(terrainAlbedoSampler, uv), roughnessMap.Sample(terrainRoughnessSampler, uv), f0, viewXform[2], n, normalize(viewDir), sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}