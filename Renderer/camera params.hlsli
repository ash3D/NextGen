#pragma once

namespace CameraParams
{
	static const float
		referenceKeyValue = .1f /*radiometric (W) which is ~ 68.3 Nit*/,
		maxExposureCompensation = /*+/-*/2 /*stops relative to referenceKeyValue*/,
		exposureCompensationDamping = 5 /*flattens out compensation growth speed throughout entire range*/,
		normalizedMiddleGray = .18f /*for linear 10/16-bit color space*/,
		normFactor = normalizedMiddleGray / referenceKeyValue /*maps referenceKeyValue to display normalizedMiddleGray*/,
		whitePointShift = +1 /*stops relative to max scene luminance*/,
		sensorSaturation = ldexp(normalizedMiddleGray, +5 /*stops*/) /*normalized*/,
		maxExposureOffset = /*+/-*/12 /*stops*/,
		maxApertureOffset = /*+/-*/4 /*stops*/,
		apertureSaturation = /*+/-*/8 /*stops of exposure offset reaching maxApertureOffset*/,
		aperturePriority = maxApertureOffset / apertureSaturation /*aperture contribution to exposure offset*/;

#ifndef __cplusplus
#if 0
	// DXC crash
	static const float2 exposureLimits = ldexp(normFactor, float2(-maxExposureOffset, +maxExposureOffset));
	static const float2 apertureLimits = exp2(float2(-maxApertureOffset, +maxApertureOffset));
#else
	static const float exposureLimits[2] = { ldexp(normFactor, float2(-maxExposureOffset, +maxExposureOffset)) };
	static const float apertureLimits[2] = { exp2(float2(-maxApertureOffset, +maxApertureOffset)) };
#endif
#endif

	struct Settings
	{
		float relativeExposure, whitePoint, exposure, aperture, whitePointFactor /*1/whitePoint^2*/;
	};
}