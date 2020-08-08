#include "CS config.hlsli"

SamplerState tapFilter : register(s0);
Texture2D src : register(t9);
RWTexture2D<float4> dst[5] : register(u6, space1);
cbuffer LOD : register(b2)
{
	uint lod;
}

// 13-tap filter
[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	float2 dstSize;
	dst[lod].GetDimensions(dstSize.x, dstSize.y);
	const float2 center = (coord + .5f) / dstSize;

	float3 acc = src.SampleLevel(tapFilter, center, lod) * .125f;
	
	acc += src.SampleLevel(tapFilter, center, lod, int2(-2, -2)) * .03125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+2, -2)) * .03125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(-2, +2)) * .03125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+2, +2)) * .03125f;

	acc += src.SampleLevel(tapFilter, center, lod, int2(-2, 0)) * .0625f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+2, 0)) * .0625f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(0, -2)) * .0625f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(0, +2)) * .0625f;

	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, -1)) * .125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, -1)) * .125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, +1)) * .125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, +1)) * .125f;

	dst[lod][coord] = float4(acc, 1);
}