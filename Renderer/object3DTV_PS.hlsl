#define ENABBLE_TEX

enum TextureID
{
	TV_SCREEN,
};

#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D material.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "samplers.hlsli"
#include "lighting.hlsli"

ConstantBuffer<TonemapParams> tonemapParams : register(b1, space1);

float4 main(in XformedVertex_UV input, in bool front : SV_IsFrontFace) : SV_TARGET
{
	input.N = normalize(front ? +input.N : -input.N);	// handles two-sided materials
	input.viewDir = normalize(input.viewDir);
	FixNormal(input.N, input.viewDir);

	const float3 screenEmission = SelectTexture(TV_SCREEN).Sample(obj3DTVSampler, input.uv) * TVBrighntess;
	const float3 color = TVBrighntess + Lit(albedo, .5f, F0(1.55f), input.N, input.viewDir, sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}