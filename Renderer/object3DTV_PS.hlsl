namespace Materials
{
	enum TextureID
	{
		TV_SCREEN,
	};
}

#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D material params.hlsli"
#include "object3D tex stuff.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "normals.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<Tonemapping::TonemapParams> tonemapParams : register(b0, space1);
ConstantBuffer<Materials::TV> materialParams : register(b1, space1);

float4 main(in XformedVertex_UV input, in bool front : SV_IsFrontFace) : SV_TARGET
{
	//using namespace Lighting;
	//using namespace Materials;

	input.viewDir = normalize(input.viewDir);
	const float3 N = Normals::EvaluateSurfaceNormal(input.N, input.viewDir, front);
	const float roughness = .5f, f0 = Fresnel::F0(1.55f);

	const float2 shadeAALevel = Lighting::EvaluateAALevel(roughness, N);
	float3 shadeResult;
#	define ShadeRegular				Lighting::Lit(materialParams.albedo, roughness, f0, N,																										input.viewDir, sun.dir, sun.irradiance)
#	define ShadeAA(sampleOffset)	Lighting::Lit(materialParams.albedo, roughness, f0, Normals::EvaluateSurfaceNormal(EvaluateAttributeSnapped(input.N, sampleOffset), input.viewDir, front),	input.viewDir, sun.dir, sun.irradiance)
#	include "shade SSAA.hlsli"

	const float3 screenEmission = SelectTexture(Materials::TV_SCREEN).Sample(Materials::TV_sampler, input.uv) * materialParams.TVBrighntess;
	shadeResult += screenEmission;

	return EncodeHDR(shadeResult, tonemapParams.exposure);
}