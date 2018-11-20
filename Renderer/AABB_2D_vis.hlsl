#include "tonemap params.hlsli"

cbuffer Constants : register(b0, space1)
{
	float3 color;
}

ConstantBuffer<TonemapParams> tonemapParams : register(b1, space1);

float4 main() : SV_TARGET
{
	return float4(color, tonemapParams.exposure);
}