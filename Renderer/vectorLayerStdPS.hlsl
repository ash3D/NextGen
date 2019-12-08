#include "per-frame data.hlsli"
#include "camera params.hlsli"
#include "normals.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"
#include "terrain samplers.hlsli"

ConstantBuffer<CameraParams::Settings> cameraSettings : register(b0, space1);

cbuffer Material : register(b1, space1)
{
	float f0;
}

Texture2D albedoMap : register(t0), roughnessMap : register(t1), normalMap : register(t2);

float4 main(in float3 viewDir : ViewDir, in float2 uv : UV) : SV_TARGET
{
	viewDir = normalize(viewDir);
	const float3 n = Normals::DoTerrainNormalMapping(terrainWorldXform, viewXform, viewDir, normalMap, TerrainSamplers::bump, uv);
	const float3 albedo = albedoMap.Sample(TerrainSamplers::albedo, uv);
	const float roughness = roughnessMap.Sample(TerrainSamplers::roughness, uv);

	const float2 shadeAALevel = Lighting::EvaluateAALevel(roughness, n);
	float3 shadeResult;
#	define SHADE_AA_ENABLE_LOD_BIAS
#	define ShadeRegular						Lighting::Lit(albedo, roughness, f0, viewXform[2], n, viewDir, sun.dir, sun.irradiance)
#	define ShadeAA(sampleOffset, lodBias)	Lighting::Lit(albedo, roughness, f0, viewXform[2],																			\
		Normals::DoTerrainNormalMapping(terrainWorldXform, viewXform, viewDir, normalMap, TerrainSamplers::bump, EvaluateAttributeSnapped(uv, sampleOffset), lodBias),	\
		viewDir, sun.dir, sun.irradiance)
#	include "shade SSAA.hlsli"

	return EncodeHDR(shadeResult, cameraSettings.exposure);
}