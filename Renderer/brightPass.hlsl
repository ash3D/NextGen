#include "CS config.hlsli"
#include "camera params.hlsli"
#include "HDR codec.hlsli"
#include "luminance.hlsli"

static float lensBloomStrength = 2e-1f;

SamplerState tapFilter : register(s0);
Texture2D src : register(t2);
RWTexture2D<float4> dst : register(u5, space1);

// downsample + bloom bright pass
[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	float2 dstSize;
	dst.GetDimensions(dstSize.x, dstSize.y);
	const float2 center = (coord + .5f) / dstSize;

	// downsample to halfres with 13-tap partial Karis filter

	float4 block =
		src.SampleLevel(tapFilter, center, 0, int2(-1, -1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(+1, -1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(-1, +1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(+1, +1)) * .25f;

	float3 color = DecodeHDR(block, .5f/*block weight*/);

	// shared taps
	const float4
		C = src.SampleLevel(tapFilter, center, 0) * .25f,
		W = src.SampleLevel(tapFilter, center, 0, int2(-2, 0)),
		E = src.SampleLevel(tapFilter, center, 0, int2(+2, 0)),
		N = src.SampleLevel(tapFilter, center, 0, int2(0, -2)),
		S = src.SampleLevel(tapFilter, center, 0, int2(0, +2));

	block = src.SampleLevel(tapFilter, center, 0, int2(-2, -2)) * .25f + C + W * .25f + N * .25f;
	color += DecodeHDR(block, .125f/*block weight*/);

	block = src.SampleLevel(tapFilter, center, 0, int2(+2, -2)) * .25f + C + E * .25f + N * .25f;
	color += DecodeHDR(block, .125f/*block weight*/);

	block = src.SampleLevel(tapFilter, center, 0, int2(-2, +2)) * .25f + C + W * .25f + S * .25f;
	color += DecodeHDR(block, .125f/*block weight*/);

	block = src.SampleLevel(tapFilter, center, 0, int2(+2, +2)) * .25f + C + E * .25f + S * .25f;
	color += DecodeHDR(block, .125f/*block weight*/);

	const float
		lum = RGB_2_luminance(color),
		sensorOverflow = lum - CameraParams::sensorSaturation,
		bloomAmount = saturate(sensorOverflow / 2) + lensBloomStrength;
	dst[coord] = float4(bloomAmount * color, 1);
}