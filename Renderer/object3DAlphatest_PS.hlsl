#define ENABBLE_TEX

namespace Materials
{
	enum TextureID
	{
		ALBEDO_MAP,
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

static const float alphaThreshold = .5f;

#if 1
// workaround for SM 6.0
float4 main(in XformedVertex_UV input, in bool front : SV_IsFrontFace, out uint sampleMask : SV_Coverage, in uint coverageMask : SV_Coverage) : SV_TARGET
{
	sampleMask = coverageMask;
#else
float4 main(in XformedVertex_UV input, in bool front : SV_IsFrontFace, inout uint sampleMask : SV_Coverage) : SV_TARGET
{
#endif
	//using namespace Lighting;
	//using namespace Materials;

	float3 albedo = 0;
	// unrolled loop with known sample count could be faster\
	it's also unclear how compiler handles divergent tex lookup without loop unrolling
	for (uint i = 0, curSampleBit = 1; curSampleBit <= sampleMask; i++, curSampleBit <<= 1u)
	{
		if (sampleMask & curSampleBit)
		{
			const float4 curSampleFetch = SelectTexture(Materials::ALBEDO_MAP).Sample(SelectSampler(TextureSamplers::OBJ3D_ALBEDO_ALPHATEST_SAMPLER), EvaluateAttributeAtSample(input.uv, i));
			if (curSampleFetch.a >= alphaThreshold)
				albedo += curSampleFetch;
			else
				sampleMask &= ~curSampleBit;
		}
	}

	if (!sampleMask) discard;			// early out
	albedo /= countbits(sampleMask);	// weighting

	input.viewDir = normalize(input.viewDir);
	const float3 N = Normals::EvaluateSurfaceNormal(input.N, input.viewDir, front);
	const float roughness = .5f, f0 = Fresnel::F0(1.55f);

	const float2 shadeAALevel = Lighting::EvaluateAALevel(roughness, N);
	float3 shadeResult;
#	define ShadeRegular				Lighting::Lit(albedo, roughness, f0, N,																										input.viewDir, sun.dir, sun.irradiance)
#	define ShadeAA(sampleOffset)	Lighting::Lit(albedo, roughness, f0, Normals::EvaluateSurfaceNormal(EvaluateAttributeSnapped(input.N, sampleOffset), input.viewDir, front),	input.viewDir, sun.dir, sun.irradiance)
#	include "shade SSAA.hlsli"

	return EncodeHDR(shadeResult, tonemapParams.exposure);
}