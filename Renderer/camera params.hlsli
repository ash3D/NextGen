#pragma once

namespace CameraParams
{
	static const float
		referenceKeyValue = .1f /*radiometric (W) which is ~ 68.3 Nit*/,
		maxExposureCompensation = /*+/-*/2 /*stops relative to referenceKeyValue*/,
		exposureCompensationDamping = 5 /*flattens out compensation growth speed throughout entire range*/,
		normalizedMiddleGray = .18f /*for linear 10/16-bit color space*/,
		normFactor = normalizedMiddleGray / referenceKeyValue /*maps referenceKeyValue to display normalizedMiddleGray*/,
		sensorSaturation = ldexp(normalizedMiddleGray, +5 /*stops*/) /*normalized*/,
		maxExposureOffset = /*+/-*/12 /*stops*/;

#if 0
	// DXC crash
	static const float2 exposureLimits = ldexp(normFactor, float2(-maxExposureOffset, +maxExposureOffset));
#else
	static const float exposureLimits[2] = { ldexp(normFactor, float2(-maxExposureOffset, +maxExposureOffset)) };
#endif

	struct Settings
	{
		float exposure, whitePoint, whitePointFactor /*1/whitePoint^2*/;
	};
}