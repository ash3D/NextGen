namespace Materials
{
	enum TextureID
	{
		ALBEDO_MAP,
	};
}

#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D tex stuff.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "normals.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<Tonemapping::TonemapParams> tonemapParams : register(b0, space1);

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
	// unrolled loop with known sample count could be faster (maybe, depends on GPU arch), but PSO compiler and GPU driver shader recompiler should have all the data to do this
	for (uint i = 0; i < GetRenderTargetSampleCount(); i++)
	{
		/*
		lift texture fetch out of 'if' to eliminate intra-quad divergent lookups and associated texcoord gradients issues
		it's actually only texcoords needed to be calculated out-of-if, fetches itself (its results) not required for inactive pixels
		so it's subject to HLSL specs if tex lookups must be quad-uniform
		need to investigate it and move tex fetch inside 'if' to enable possible speedups
		though this optimization may have no effect due to highly divergent branching on polygon edges, for the same reason flatten can be beneficial (need to experiment with this too)
		*/
		const float4 curSampleFetch = SelectTexture(Materials::ALBEDO_MAP).Sample(SelectSampler(TextureSamplers::OBJ3D_ALBEDO_ALPHATEST_SAMPLER), EvaluateAttributeAtSample(input.uv, i));
		const uint curSampleBit = 1u << i;
		if (sampleMask & curSampleBit)
		{
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