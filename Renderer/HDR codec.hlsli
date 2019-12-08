#pragma once

#include "luminance.hlsli"
#include "camera params.hlsli"

namespace HDRImpl
{
	static const float fp16Range = 64E3f/*65504.f, leave some room for fp32 precision looses*/, HDRAlphaRescale = fp16Range / CameraParams::exposureLimits[1];
}

/*
	simple Reinhard tonemapping for proper hardware MSAA resolve

	it is possible to do multiplication in ROPs (blending) to free general purpose shader ALUs
	but turning blending on can hurt performance and reduce precision (fp16 vs fp32)

	rescale alpha to utilize fp16 range more fully and prevent flush to 0 for bright pixels (which leads to inf after decode)
	current rescale is quite limited due to necessity to honor exposure limits
	far more aggressive rescale (full fp16 range) could be applied if not mul alpha by exposure and instead do it during decode
	it would require reading preexposure from CB in tonemapping passes and keeping it along with actual calculated exposure for current frame
*/
float4 EncodeHDR(float3 color, float exposure)
{
	color *= exposure;
	const float reinhardFactor = rcp(1 + RGB_2_luminance(color));
	return float4(color * reinhardFactor, reinhardFactor/*[0..1]*/ * exposure/*[exposureLimits[0]..exposureLimits[1]]*/ * HDRImpl::HDRAlphaRescale);
}

float4 EncodeLDR(float3 color, float exposure)
{
	return float4(color, exposure * HDRImpl::HDRAlphaRescale);
}

namespace HDRImpl
{
	inline float3 DecodeHDR(float4 encodedPixel, float factor)
	{
		return encodedPixel.rgb * (factor / encodedPixel.a);
	}
}

float3 DecodeHDR(float4 encodedPixel)
{
	return HDRImpl::DecodeHDR(encodedPixel, HDRImpl::HDRAlphaRescale);
}

// decode + apply scale (e.g. exposure)
float3 DecodeHDR(float4 encodedPixel, float scale)
{
	return HDRImpl::DecodeHDR(encodedPixel, scale * HDRImpl::HDRAlphaRescale);
}