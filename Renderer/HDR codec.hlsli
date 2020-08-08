#pragma once

#include "luminance.hlsli"
#include "camera params.hlsli"

namespace HDRImpl
{
	static const float
		fp16Range[2] = { 7e-5f/*or denormalized ~5.96e-8f -> 6e-8f ?*/, 64E+3f }/*[~6.1e-5f..65504.f], leave some room for fp32 precision looses*/,
		HDRAlphaRescale = fp16Range[1] / CameraParams::exposureLimits[1],
		reinhardLimit = fp16Range[0] / fp16Range[1],
		reinhardExpLimit = fp16Range[0] / (CameraParams::exposureLimits[0] * HDRAlphaRescale);

	inline float4 EncodeHDR(float3 color, float factor, uniform float reinhardFactorLimit)
	{
		const float reinhardFactor = max(rcp(1 + RGB_2_luminance(color)), reinhardFactorLimit);
		color *= reinhardFactor;	// inf -> NaN
		factor *= reinhardFactor/*[0..1]*/;
#if 0
		color = min(color, 1);	// NaN -> 1 -> inf in decode
#endif
		return float4(color, factor);
	}

	inline float3 DecodeHDR(float4 encodedPixel, float factor)
	{
		return encodedPixel.rgb * (factor / encodedPixel.a);
	}
}

/*
	simple Reinhard tonemapping for proper hardware MSAA resolve

	it is possible to do multiplication in ROPs (blending) to free general purpose shader ALUs
	but turning blending on can hurt performance and reduce precision (fp16 vs fp32)

	rescale alpha to utilize fp16 range more fully and prevent flush to 0 for bright pixels (which leads to inf after decode)
	current rescale is quite limited due to necessity to honor exposure limits
	far more aggressive rescale (full fp16 range) could be applied if not mul alpha by exposure and instead do it during decode
	it would require reading preexposure from CB in decoding passes and keeping it along with actual calculated exposure for current frame
*/
float4 EncodeHDRExp(float3 color, float exposure)
{
	return HDRImpl::EncodeHDR(color * exposure, exposure/*[exposureLimits[0]..exposureLimits[1]]*/ * HDRImpl::HDRAlphaRescale, HDRImpl::reinhardExpLimit);
}

float4 EncodeHDR(float3 color)
{
	return HDRImpl::EncodeHDR(color, HDRImpl::fp16Range[1], HDRImpl::reinhardLimit);
}

float4 EncodeLDRExp(float3 color, float exposure)
{
	return float4(color, exposure * HDRImpl::HDRAlphaRescale);
}

float3 DecodeHDRExp(float4 encodedPixel)
{
	return HDRImpl::DecodeHDR(encodedPixel, HDRImpl::HDRAlphaRescale);
}

// decode + apply scale (e.g. exposure)
float3 DecodeHDRExp(float4 encodedPixel, float scale)
{
	return HDRImpl::DecodeHDR(encodedPixel, scale * HDRImpl::HDRAlphaRescale);
}

float3 DecodeHDR(float4 encodedPixel)
{
	return HDRImpl::DecodeHDR(encodedPixel, HDRImpl::fp16Range[1]);
}

// decode + apply scale
float3 DecodeHDR(float4 encodedPixel, float scale)
{
	return HDRImpl::DecodeHDR(encodedPixel, scale * HDRImpl::fp16Range[1]);
}