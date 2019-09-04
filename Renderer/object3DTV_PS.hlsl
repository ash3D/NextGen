#define ENABBLE_TEX

namespace Materials
{
	enum TextureID
	{
		TV_SCREEN,
	};
}

#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "object3D material.hlsli"
#include "object3D VS 2 PS.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<Tonemapping::TonemapParams> tonemapParams : register(b1, space1);

float4 main(in XformedVertex_UV input, in bool front : SV_IsFrontFace) : SV_TARGET
{
	//using namespace Lighting;
	//using namespace Materials;

	input.N = normalize(front ? +input.N : -input.N);	// handles two-sided materials
	input.viewDir = normalize(input.viewDir);
	Lighting::FixNormal(input.N, input.viewDir);

	const float3 screenEmission = SelectTexture(Materials::TV_SCREEN).Sample(Materials::TV_sampler, input.uv) * TVBrighntess;
	const float3 color = screenEmission + Lighting::Lit(albedo, .5f, Fresnel::F0(1.55f), input.N, input.viewDir, sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}