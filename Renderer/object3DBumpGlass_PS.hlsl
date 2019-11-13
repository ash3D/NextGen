namespace Materials
{
	enum TextureID
	{
		ALBEDO_MAP,
		NORMAL_MAP,
		GLASS_MASK,
	};
}

#include "per-frame data.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "object3D tex stuff.hlsli"
#include "object3D material params.hlsli"
#include "tonemap params.hlsli"
#include "glass.hlsli"
#include "normals.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<Materials::Common>			materialParams	: register(b0, space1);
ConstantBuffer<Tonemapping::TonemapParams>	tonemapParams	: register(b0, space2);

inline float3 Lit(float3 albedo, float roughness, float F0, Normals::Nn Nn, float3 viewDir, float3 lightDir, float3 lightIrradiance)
{
	return Lighting::Lit(albedo, roughness, F0, Nn.N, Nn.n, viewDir, lightDir, lightIrradiance);
}

float4 main(in XformedVertexBump input, in bool front : SV_IsFrontFace) : SV_TARGET
{
	//using namespace Lighting;
	//using namespace Materials;

	input.viewDir = normalize(input.viewDir);
	const Normals::Nn Nn = Normals::DoNormalMapping(input.tangents[0], input.tangents[1], input.N, input.viewDir, front, SelectTexture(Materials::NORMAL_MAP), SelectSampler(TextureSamplers::OBJ3D_BUMP_SAMPLER), input.uv, input.normalScale);
	const float3 albedo = SelectTexture(Materials::ALBEDO_MAP).Sample(SelectSampler(TextureSamplers::OBJ3D_ALBEDO_SAMPLER), input.uv);
	float roughness = materialParams.roughness, f0 = materialParams.f0;
	Materials::ApplyGlassMask(input.uv, roughness, f0);

	const float2 shadeAALevel = Lighting::EvaluateAALevel(roughness, Nn.n);
	float3 shadeResult;
#	define SHADE_AA_ENABLE_LOD_BIAS
#	define ShadeRegular						Lighting::Lit(albedo, roughness, f0, Nn.N, Nn.n, input.viewDir, sun.dir, sun.irradiance)
#	define ShadeAA(sampleOffset, lodBias)	Lit(albedo, roughness, f0,																																		\
		Normals::DoNormalMapping(																																											\
			EvaluateAttributeSnapped(input.tangents[0], sampleOffset), EvaluateAttributeSnapped(input.tangents[1], sampleOffset), EvaluateAttributeSnapped(input.N, sampleOffset),							\
			input.viewDir, front, SelectTexture(Materials::NORMAL_MAP), SelectSampler(TextureSamplers::OBJ3D_BUMP_SAMPLER), EvaluateAttributeSnapped(input.uv, sampleOffset), input.normalScale, lodBias),	\
		input.viewDir, sun.dir, sun.irradiance)
#	include "shade SSAA.hlsli"

	return EncodeHDR(shadeResult, tonemapParams.exposure);
}