#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "object3D tex materials.hlsli"
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
	const Materials::Attribs materialAttribs = Materials::EvaluateGenericMaterial(input.uv, true);

	const float2 shadeAALevel = Lighting::EvaluateAALevel(materialAttribs.roughness, N);
	float3 shadeResult;
#	define ShadeRegular				Lighting::Lit(materialAttribs.albedo, materialAttribs.roughness, materialAttribs.f0, N,																										input.viewDir, sun.dir, sun.irradiance)
#	define ShadeAA(sampleOffset)	Lighting::Lit(materialAttribs.albedo, materialAttribs.roughness, materialAttribs.f0, Normals::EvaluateSurfaceNormal(EvaluateAttributeSnapped(input.N, sampleOffset), input.viewDir, front),	input.viewDir, sun.dir, sun.irradiance)
#	include "shade SSAA.hlsli"

	return EncodeHDR(shadeResult, tonemapParams.exposure);
}