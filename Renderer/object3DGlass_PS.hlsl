namespace Materials
{
	enum TextureID
	{
		ALBEDO_MAP,
		GLASS_MASK,
	};
}

#include "per-frame data.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "object3D tex stuff.hlsli"
#include "object3D material params.hlsli"
#include "camera params.hlsli"
#include "glass.hlsli"
#include "normals.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<Materials::Common>		materialParams	: register(b0, space1);
ConstantBuffer<CameraParams::Settings>	cameraSettings	: register(b0, space2);

float4 main(in XformedVertexTex input, in bool front : SV_IsFrontFace) : SV_TARGET
{
	//using namespace Lighting;
	//using namespace Materials;

	input.viewDir = normalize(input.viewDir);
	const float3 N = Normals::EvaluateSurfaceNormal(input.N, input.viewDir, front);
	const float3 albedo = SelectTexture(Materials::ALBEDO_MAP).Sample(SelectSampler(TextureSamplers::OBJ3D_ALBEDO_SAMPLER), input.uv);
	float roughness = materialParams.roughness, f0 = materialParams.f0;
	Materials::ApplyGlassMask(input.uv, roughness, f0);

	const float2 shadeAALevel = Lighting::EvaluateAALevel(roughness, N);
	float3 shadeResult;
#	define ShadeRegular				Lighting::Lit(albedo, roughness, f0, N,																										input.viewDir, sun.dir, sun.irradiance)
#	define ShadeAA(sampleOffset)	Lighting::Lit(albedo, roughness, f0, Normals::EvaluateSurfaceNormal(EvaluateAttributeSnapped(input.N, sampleOffset), input.viewDir, front),	input.viewDir, sun.dir, sun.irradiance)
#	include "shade SSAA.hlsli"

	return EncodeHDRExp(shadeResult, cameraSettings.exposure);
}