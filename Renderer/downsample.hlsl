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
	float2 srcSize;
	float mips;
	src.GetDimensions(lod, srcSize.x, srcSize.y, mips);
	const float2 centerPoint = (coord * 2 + 1) / srcSize;

	float3 acc = src.SampleLevel(tapFilter, centerPoint, lod) * .125f;
	
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(-2, -2)) * .03125f;
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(+2, -2)) * .03125f;
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(-2, +2)) * .03125f;
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(+2, +2)) * .03125f;

	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(-2, 0)) * .0625f;
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(+2, 0)) * .0625f;
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(0, -2)) * .0625f;
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(0, +2)) * .0625f;

	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(-1, -1)) * .125f;
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(+1, -1)) * .125f;
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(-1, +1)) * .125f;
	acc += src.SampleLevel(tapFilter, centerPoint, lod, int2(+1, +1)) * .125f;

	dst[lod][coord] = float4(acc, 1);
}