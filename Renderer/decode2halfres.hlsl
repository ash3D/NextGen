#include "CS config.hlsli"
#include "tonemap params.hlsli"
#include "HDR codec.hlsli"

SamplerState tapFilter : register(s0);
Texture2D src : register(t0);
RWTexture2D<float4> dst : register(u0, space1);
ConstantBuffer<Tonemapping::TonemapParams> tonemapParams : register(b0);

// 13-tap partial Karis filter
[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	float2 dstSize;
	dst.GetDimensions(dstSize.x, dstSize.y);
	const float2 center = (coord + .5f) / dstSize;

	float4 block =
		src.SampleLevel(tapFilter, center, 0, int2(-1, -1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(+1, -1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(-1, +1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(+1, +1)) * .25f;

	float3 acc = DecodeHDRExp(block, .5f/*block weight*/ * tonemapParams.exposure);

	// shared taps
	const float4
		C = src.SampleLevel(tapFilter, center, 0) * .25f,
		W = src.SampleLevel(tapFilter, center, 0, int2(-2, 0)),
		E = src.SampleLevel(tapFilter, center, 0, int2(+2, 0)),
		N = src.SampleLevel(tapFilter, center, 0, int2(0, -2)),
		S = src.SampleLevel(tapFilter, center, 0, int2(0, +2));

	block = src.SampleLevel(tapFilter, center, 0, int2(-2, -2)) * .25f + C + W * .25f + N * .25f;
	acc += DecodeHDRExp(block, .125f/*block weight*/ * tonemapParams.exposure);

	block = src.SampleLevel(tapFilter, center, 0, int2(+2, -2)) * .25f + C + E * .25f + N * .25f;
	acc += DecodeHDRExp(block, .125f/*block weight*/ * tonemapParams.exposure);

	block = src.SampleLevel(tapFilter, center, 0, int2(-2, +2)) * .25f + C + W * .25f + S * .25f;
	acc += DecodeHDRExp(block, .125f/*block weight*/ * tonemapParams.exposure);

	block = src.SampleLevel(tapFilter, center, 0, int2(+2, +2)) * .25f + C + E * .25f + S * .25f;
	acc += DecodeHDRExp(block, .125f/*block weight*/ * tonemapParams.exposure);

	dst[coord] = float4(acc, 1);
}