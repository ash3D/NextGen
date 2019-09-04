#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"
#include "terrain samplers.hlsli"

ConstantBuffer<Tonemapping::TonemapParams> tonemapParams : register(b0, space1);

cbuffer Material : register(b1, space1)
{
	float f0;
}

Texture2D albedoMap : register(t0), roughnessMap : register(t1), normalMap : register(t2);

float4 main(in float3 viewDir : ViewDir, in float2 uv : UV) : SV_TARGET
{
	/*
		TBN space for terrain in terrain space is
		{1, 0, 0}
		{0, 1, 0}
		{0, 0, -1}
		terrain 'up' inverted so -1 for N
	*/
	float3 n;
	n.xy = normalMap.Sample(TerrainSamplers::bump, uv);
	n.z = -sqrt(saturate(1.f - length(n.xy)));

	/*
		xform normal 'terrain space' -> 'view space'
		similar note as for Flat shader applicable:
			both terrainWorldXform and viewXform assumed to be orthonormal, need inverse transpose otherwise
			mul with terrainWorldXform is suboptimal: get rid of terrain xform at all or merge in worldViewXform
	*/
	n = mul(mul(n, terrainWorldXform), viewXform);

	const float3 color = Lighting::Lit(albedoMap.Sample(TerrainSamplers::albedo, uv), roughnessMap.Sample(TerrainSamplers::roughness, uv), f0, viewXform[2], n, normalize(viewDir), sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}