#define ENABLE_TEX

namespace Materials
{
	enum TextureID
	{
		TV_SCREEN,
	};
}

#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D materials common.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "normals.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<Tonemapping::TonemapParams> tonemapParams : register(b1, space1);

float4 main(in XformedVertex_UV input, in bool front : SV_IsFrontFace) : SV_TARGET
{
	//using namespace Lighting;
	//using namespace Materials;

	input.viewDir = normalize(input.viewDir);
	const float3 N = Normals::EvaluateSurfaceNormal(input.N, input.viewDir, front);
	const float roughness = .5f, f0 = Fresnel::F0(1.55f);

	const float2 shadeAALevel = Lighting::EvaluateAALevel(roughness, N);
	float3 shadeResult;
#	define ShadeRegular				Lighting::Lit(albedo, roughness, f0, N,																										input.viewDir, sun.dir, sun.irradiance)
#	define ShadeAA(sampleOffset)	Lighting::Lit(albedo, roughness, f0, Normals::EvaluateSurfaceNormal(EvaluateAttributeSnapped(input.N, sampleOffset), input.viewDir, front),	input.viewDir, sun.dir, sun.irradiance)
#	include "shade SSAA.hlsli"

	const float3 screenEmission = SelectTexture(Materials::TV_SCREEN).Sample(Materials::TV_sampler, input.uv) * TVBrighntess;
	shadeResult += screenEmission;

	return EncodeHDR(shadeResult, tonemapParams.exposure);
}