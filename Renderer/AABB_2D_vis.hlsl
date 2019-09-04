#include "tonemap params.hlsli"
#include "HDR codec.hlsli"

cbuffer Constants : register(b0, space1)
{
	float3 color;
}

ConstantBuffer<Tonemapping::TonemapParams> tonemapParams : register(b1, space1);

float4 main() : SV_TARGET
{
	return EncodeLDR(color, tonemapParams.exposure);
}