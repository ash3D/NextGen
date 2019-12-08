#include "CS config.hlsli"
#include "tonemap params.hlsli"
#include "luminance.hlsli"
#include "HDR codec.hlsli"
#include "upsampleBlur.hlsli"

#define DXC_NAMESPACE_WORKAROUND 1

#if DXC_NAMESPACE_WORKAROUND
SamplerState tapFilter : register(s0);
Texture2D src : register(t0);
RWTexture2D<float4> dst : register(u0);
Texture2D bloom : register(t2);
#endif

namespace Tonemapping
{
	SamplerState tapFilter : register(s0);
	Texture2D src : register(t0);
	RWTexture2D<float4> dst : register(u0);
	Texture2D bloom : register(t2);
	ConstantBuffer<TonemapParams> tonemapParams : register(b0);

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

	float2 dstSize;
	dst.GetDimensions(dstSize.x, dstSize.y);
	const float2 center = (coord + .5f) / dstSize;

	float3 exposedPixel = DecodeHDRExp(src[coord], Tonemapping::tonemapParams.exposure);
	exposedPixel += UpsampleBlur(bloom, tapFilter, center, 0) / 6;
	// Reinhard maps inf to NaN (inf/inf), 'min' converts it to large value
	dst[coord] = float4(min(Tonemapping::Reinhard(exposedPixel, Tonemapping::tonemapParams.whitePointFactor), 64e3f), 1);
}