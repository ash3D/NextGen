#ifndef TONEMAP_PARAMS_INCLUDED
#define TONEMAP_PARAMS_INCLUDED

static const float
	referenceKeyValue = .1f /*radiomentric (W) which is ~ 68.3 Nit*/,
	maxExposureCompensation = /*+/-*/2 /*stops relative to referenceKeyValue*/,
	normalizedMiddleGray = .18f /*for linear 10/16-bit color space*/,
	normFactor = normalizedMiddleGray / referenceKeyValue /*maps referenceKeyValue to display normalizedMiddleGray*/,
	sensorSaturation = ldexp(normalizedMiddleGray, +5 /*stops*/) /*normalized*/,
	maxExposureOffset = /*+/-*/12 /*stops*/;

static const float2 exposureLimits = ldexp(normFactor, float2(-maxExposureOffset, +maxExposureOffset));

struct TonemapParams
{
    float exposure, whitePoint, whitePointFactor /*1/whitePoint^2*/;
};

#endif	// TONEMAP_PARAMS_INCLUDED