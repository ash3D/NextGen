#include "CS config.hlsli"
#include "camera params.hlsli"
#include "luminance.hlsli"
#include "HDR codec.hlsli"
#include "upsampleBlur.hlsli"

SamplerState tapFilter : register(s0);
Texture2D src : register(t2);
RWTexture2D<float4> dst : register(u1);
Texture2D bloom : register(t9);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

namespace Tonemapping
{
	float3 Reinhard(float3 color, float whitePointFactor)
	{
		const float L = RGB_2_luminance(color);
		return color * ((1 + L * whitePointFactor) / (1 + L));
	}
}

[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	//using namespace Tonemapping;

	float2 srcSize;
	bloom.GetDimensions(srcSize.x, srcSize.y);
	const float2 centerPoint = (coord * .5f + .25f) / srcSize;

	float3 exposedPixel = DecodeHDR(src[coord]);
	exposedPixel += UpsampleBlur3(bloom, tapFilter, centerPoint) / 6;
	// Reinhard maps inf to NaN (inf/inf), 'min' converts it to large value
	dst[coord] = float4(min(Tonemapping::Reinhard(exposedPixel, cameraSettings.whitePointFactor), 64E3f), 1);
}