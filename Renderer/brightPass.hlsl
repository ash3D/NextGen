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
	float2 srcSize;
	src.GetDimensions(srcSize.x, srcSize.y);
	const float2 centerPoint = (coord * 2 + 1) / srcSize;

	// downsample to halfres with 13-tap partial Karis filter

	float4 block =
		src.SampleLevel(tapFilter, centerPoint, 0, int2(-1, -1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(+1, -1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(-1, +1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(+1, +1));

	float3 color = DecodeHDR(block, .5f/*block weight*/);

	// shared taps
	const float4
		C = src.SampleLevel(tapFilter, centerPoint, 0),
		W = src.SampleLevel(tapFilter, centerPoint, 0, int2(-2, 0)),
		E = src.SampleLevel(tapFilter, centerPoint, 0, int2(+2, 0)),
		N = src.SampleLevel(tapFilter, centerPoint, 0, int2(0, -2)),
		S = src.SampleLevel(tapFilter, centerPoint, 0, int2(0, +2));

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(-2, -2)) + C + W + N;
	color += DecodeHDR(block, .125f/*block weight*/);

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(+2, -2)) + C + E + N;
	color += DecodeHDR(block, .125f/*block weight*/);

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(-2, +2)) + C + W + S;
	color += DecodeHDR(block, .125f/*block weight*/);

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(+2, +2)) + C + E + S;
	color += DecodeHDR(block, .125f/*block weight*/);

	const float
		lum = RGB_2_luminance(color),
		sensorOverflow = lum - CameraParams::sensorSaturation,
		bloomAmount = saturate(sensorOverflow / 2) + lensBloomStrength;
	dst[coord] = float4(bloomAmount * color, 1);
}