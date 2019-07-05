#include "tonemapping config.hlsli"
#include "tonemap params.hlsli"
#include "luminance.hlsli"
#include "HDR codec.hlsli"

Texture2D src : register(t0);
RWTexture2D<float4> dst : register(u0);
ConstantBuffer<TonemapParams> tonemapParams : register(b0);

float3 Reinhard(float3 color, float whitePointFactor)
{
	const float L = RGB_2_luminance(color);
	return color * ((1 + L * whitePointFactor) / (1 + L));
}

[numthreads(blockSize, blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	const float3 exposedPixel = DecodeHDRExp(src[coord], tonemapParams.exposure);
	
	// Reinhard maps inf to NaN (inf/inf), 'min' converts it to large value
	dst[coord] = float4(min(Reinhard(exposedPixel, tonemapParams.whitePointFactor), 64e3f), 1);
}