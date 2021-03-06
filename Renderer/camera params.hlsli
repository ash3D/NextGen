#pragma once

namespace CameraParams
{
	static const float
		referenceKeyValue = .1f /*radiometric (W) which is ~ 68.3 Nit*/,
		maxExposureCompensation = /*+/-*/2 /*stops relative to referenceKeyValue*/,
		exposureCompensationDamping = 5 /*flattens out compensation growth rate throughout entire range*/,
		normalizedMiddleGray = .18f /*for linear 10/16-bit color space*/,
		normFactor = normalizedMiddleGray / referenceKeyValue /*maps referenceKeyValue to display normalizedMiddleGray*/;

#ifndef __cplusplus
	static const float
		whitePointShift = +1 /*stops relative to max scene luminance*/,
		sensorSaturation = ldexp(normalizedMiddleGray, +5 /*stops*/) /*normalized*/,
		maxExposureOffset = /*+/-*/12 /*stops*/,
		maxApertureOffset = /*+/-*/4 /*stops*/,
		apertureLimit = exp2(-maxApertureOffset) /*normalized aperture range [apertureLimit..1]*/,
		apertureSaturation = /*+/-*/10 /*stops of exposure offset reaching maxApertureOffset*/,
		aperturePriority = maxApertureOffset / apertureSaturation /*aperture contribution to exposure offset*/,
		apertureAngleRange = radians(36) /*iris aperture rotation linearly driven by radius*/,
		apertureAngleScale = apertureAngleRange / (1 - apertureLimit) /*maps [apertureLimit..1] -> [0..apertureAngleRange]*/;

	static const float2 exposureLimits = ldexp(normFactor, float2(-maxExposureOffset, +maxExposureOffset));
#endif

	struct Settings
	{
		float relativeExposure, whitePoint, sensorPlane/*focus*/, exposure, aperture, whitePointFactor /*1/whitePoint^2*/;
		float2 apertureRot;
		float2 COCParams;

#ifndef __cplusplus
		// in halfres pixels
		inline float COC(float Z)
		{
			return mad(Z, COCParams[0], COCParams[1]);
		}
#endif
	};
}