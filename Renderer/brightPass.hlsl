#include "CS config.hlsli"
#include "camera params.hlsli"
#include "luminance.hlsli"

static half lensBloomStrength = 2e-1f;

Texture2D lensFlare : register(t2);
Texture2D src : register(t3);
RWTexture2D<half4> dst : register(u5, space1);

// bloom bright pass
[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	const half3 color = src[coord] + lensFlare[coord];
	const half
		lum = RGB_2_luminance(color),
		sensorOverflow = lum - CameraParams::sensorSaturation,
		bloomAmount = saturate(sensorOverflow / 2) + lensBloomStrength;
	dst[coord] = half4(bloomAmount * color, 1);
}