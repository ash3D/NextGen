#include "per-frame data.hlsli"
#include "tonemap params.hlsli"
#include "lighting.hlsli"

ConstantBuffer<TonemapParams> tonemapParams : register(b0, space1);

cbuffer Constants : register(b1, space1)
{
	float3 albedo;
}

float4 main(in float3 viewDir : ViewDir) : SV_TARGET
{
	/*
		xform (0, 0, 1) world space normal by viewXform
		assumes viewXform is orthonormal, need inverse transpose otherwise (and normalize)
	*/
	const float3 color = Lit(albedo, .5f, F0(1.55f), viewXform[2], normalize(viewDir), sun.dir, sun.irradiance);

	return EncodeHDR(color, tonemapParams.exposure);
}