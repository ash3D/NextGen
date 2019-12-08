#include "CS config.hlsli"
#include "upsampleBlur.hlsli"

SamplerState tapFilter : register(s0);
Texture2D blurSrc : register(t2), addSrc : register(t3);
RWTexture2D<float4> dst[5] : register(u0, space1);
cbuffer LOD : register(b1)
{
	uint lod;
}

[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	float2 dstSize;
	dst[lod].GetDimensions(dstSize.x, dstSize.y);
	const float2 center = (coord + .5f) / dstSize;

	const uint blurLod = lod + 1;
	float3 color = UpsampleBlur(blurSrc, tapFilter, center, blurLod);
	color += addSrc.mips[lod][coord];

	dst[lod][coord] = float4(color, 1);
}