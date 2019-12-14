#include "CS config.hlsli"
#include "camera params.hlsli"

namespace LumAdaptaion
{
	static const uint localDataSize = CSConfig::LuminanceReduction::BufferPass::blockSize;
}
#include "luminanceLocalReduction.hlsli"

#define DXC_NAMESPACE_WORKAROUND 1

#if DXC_NAMESPACE_WORKAROUND
ByteAddressBuffer buffer : register(t1);
RWByteAddressBuffer cameraSettings : register(u2);
#endif

namespace LumAdaptaion
{
	ByteAddressBuffer buffer : register(t1);
	RWByteAddressBuffer cameraSettings : register(u2);
	cbuffer LumAdaptation : register(b1)
	{
		float lerpFactor;
	}

	inline float LinearizeLum(float src)
	{
		return exp2(src) - 1;
	}

	/*
		similar to Reinhard, maps [0, inf] -> [0, range]
		strength boosts compression near 0 - it is reciprocal of derivative
		!: can produce NaN in corner cases (0, inf, both x and range)
		alternative formulation can be used: x / (strength + abs(x) / range), which could be faster (mad + div) and behaves differently in corner cases
	*/
	inline float Compress(float x, uniform float range, uniform float strength)
	{
		return x * range / (range * strength + abs(x));
	}

	inline float Autoexposure(float avgLogLum, float lastSetting)
	{
		//using namespace CameraParams;

		const float
			sceneKeyValue = LinearizeLum(avgLogLum),
			targetKeyValue = ldexp(CameraParams::referenceKeyValue, Compress(log2(sceneKeyValue / CameraParams::referenceKeyValue), CameraParams::maxExposureCompensation, CameraParams::exposureCompensationDamping)),
			targetRelativeExposure = targetKeyValue / sceneKeyValue,
			relativeExposure = clamp(lerp(targetRelativeExposure, lastSetting, lerpFactor), CameraParams::exposureLimits[0], CameraParams::exposureLimits[1]);

		return relativeExposure;
	}

	inline float WhitePoint(float maxSceneLum, float exposure, float lastSetting)
	{
		const float
			targetWhitePoint = ldexp(maxSceneLum * exposure, CameraParams::whitePointShift),
			whitePoint = clamp(lerp(targetWhitePoint, lastSetting, lerpFactor), 1, CameraParams::sensorSaturation);

		return whitePoint;
	}

	inline float4 UpdateCameraSettings(float2 sceneLumParams, float2 lastSettings)
	{
		const float
			relativeExposure = Autoexposure(sceneLumParams[0], lastSettings[0]),
			exposure = relativeExposure * CameraParams::normFactor,
			whitePoint = WhitePoint(sceneLumParams[1], exposure, lastSettings[1]);

		return float4(relativeExposure, whitePoint, exposure, rcp(whitePoint * whitePoint));
	}
}

[numthreads(CSConfig::LuminanceReduction::BufferPass::blockSize, 1, 1)]
void main(in uint globalIdx : SV_DispatchThreadID, in uint localIdx : SV_GroupIndex)
{
	// global buffer loading combined with first level reduction
	const float4 batch = asfloat(buffer.Load4(globalIdx * 16));
	const float2 partialReduction = { batch[0] + batch[2], max(batch[1], batch[3]) };

	// bulk of reduction work
	const float2 finalReduction = LumAdaptaion::LocalReduce(partialReduction, localIdx);

	// update camera settings buffer
	if (localIdx == 0)
		cameraSettings.Store4(0, asuint(LumAdaptaion::UpdateCameraSettings(finalReduction, asfloat(cameraSettings.Load2(0)))));
}