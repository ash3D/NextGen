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

	inline void Autoexposure(in float avgLogLum, inout float lastSetting, out float aperture)
	{
		//using namespace CameraParams;

		const float
			sceneKeyValue = LinearizeLum(avgLogLum),
			targetKeyValue = ldexp(CameraParams::referenceKeyValue, Compress(log2(sceneKeyValue / CameraParams::referenceKeyValue), CameraParams::maxExposureCompensation, CameraParams::exposureCompensationDamping)),
			targetRelativeExposure = targetKeyValue / sceneKeyValue;

		lastSetting = clamp(lerp(targetRelativeExposure, lastSetting, lerpFactor), CameraParams::exposureLimits[0], CameraParams::exposureLimits[1]);

		// exp2(log2(lastSetting) * CameraParams::aperturePriority) == pow(lastSetting, CameraParams::aperturePriority)
		aperture = sqrt(clamp(pow(lastSetting, CameraParams::aperturePriority), CameraParams::apertureLimits[0], CameraParams::apertureLimits[1]));
	}

	inline void UpdateWhitePoint(in float maxSceneLum, in float exposure, inout float lastSetting)
	{
		const float targetWhitePoint = ldexp(maxSceneLum * exposure, CameraParams::whitePointShift);
		lastSetting = clamp(lerp(targetWhitePoint, lastSetting, lerpFactor), 1, CameraParams::sensorSaturation);
	}

	inline void UpdateCameraSettings(in float avgLogLum, in float maxSceneLum, inout float relativeExposure, inout float whitePoint, out float exposure, out float aperture, out float whitePointFactor)
	{
		Autoexposure(avgLogLum, relativeExposure, aperture);
		exposure = relativeExposure * CameraParams::normFactor;
		UpdateWhitePoint(maxSceneLum, exposure, whitePoint);
		whitePointFactor = rcp(whitePoint * whitePoint);
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
	{
		float2 lastSettings = asfloat(cameraSettings.Load2(0));
		float exposure, aperture, whitePointFactor;
		LumAdaptaion::UpdateCameraSettings(finalReduction[0], finalReduction[1], lastSettings[0], lastSettings[1], exposure, aperture, whitePointFactor);
		cameraSettings.Store2(0, asuint(lastSettings));
		cameraSettings.Store3(8, asuint(float3(exposure, aperture, whitePointFactor)));
	}
}