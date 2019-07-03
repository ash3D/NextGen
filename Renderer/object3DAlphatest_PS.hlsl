#define ENABBLE_TEX

enum TextureID
{
	ALBEDO_MAP,
};

#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D material.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "samplers.hlsli"
#include "lighting.hlsli"

ConstantBuffer<TonemapParams> tonemapParams : register(b1, space1);

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
	input.N = normalize(front ? +input.N : -input.N);	// handles two-sided materials
	input.viewDir = normalize(input.viewDir);
	FixNormal(input.N, input.viewDir);

	float3 albedo = 0;
	// unrolled loop with known sample count could be faster\
	it's also unclear how compiler handles divergent tex lookup without loop unrolling
	for (uint i = 0, curSampleBit = 1; curSampleBit <= sampleMask; i++, curSampleBit <<= 1u)
	{
		if (sampleMask & curSampleBit)
		{
			const float4 curSampleFetch = SelectTexture(ALBEDO_MAP).Sample(obj3DAlbedoAlphatestSampler, EvaluateAttributeAtSample(input.uv, i));
			if (curSampleFetch.a >= alphaThreshold)
				albedo += curSampleFetch;
			else
				sampleMask &= ~curSampleBit;
		}
	}

	if (!sampleMask) discard;			// early out
	albedo /= countbits(sampleMask);	// weighting

	const float3 color = Lit(albedo, .5f, F0(1.55f), input.N, input.viewDir, sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}