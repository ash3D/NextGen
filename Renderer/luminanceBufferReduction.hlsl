#include "CS config.hlsli"
#include "per-frame data.hlsli"
#include "camera params.hlsli"

namespace LumAdaptaion
{
	static const uint localDataSize = CSConfig::LuminanceReduction::BufferPass::blockSize;
}
#include "luminanceLocalReduction.hlsli"

Texture2DMS<float> ZBuffer : register(t0);
ByteAddressBuffer lumBuffer : register(t3);
RWByteAddressBuffer cameraSettings : register(u3);

namespace LumAdaptaion
{
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

	inline float LuminanceAdaptation(float target, float last)
	{
		const float lerpFactor = target > last ? camAdaptationFactors[0]/*LumBright*/ : camAdaptationFactors[1]/*LumDark*/;
		return lerp(target, last, lerpFactor);
	}

	inline void Autoexposure(in float avgLogLum, inout float lastSetting, out float aperture, out float2 apertureRot)
	{
		//using namespace CameraParams;

		const float
			sceneKeyValue = LinearizeLum(avgLogLum),
			targetKeyValue = ldexp(CameraParams::referenceKeyValue, Compress(log2(sceneKeyValue / CameraParams::referenceKeyValue), CameraParams::maxExposureCompensation, CameraParams::exposureCompensationDamping)),
			targetRelativeExposure = targetKeyValue / sceneKeyValue;

		lastSetting = clamp(LuminanceAdaptation(targetRelativeExposure, lastSetting), CameraParams::exposureLimits[0], CameraParams::exposureLimits[1]);

		// exp2(log2(lastSetting) * CameraParams::aperturePriority) == pow(lastSetting, CameraParams::aperturePriority)
		aperture = pow(lastSetting, CameraParams::aperturePriority);	// aperture contribution to exposure offset
		aperture *= CameraParams::apertureLimit;						// normalized 'max -> 1'
		aperture = sqrt(aperture);										// exposure offset -> aperture radius
		aperture = clamp(aperture, CameraParams::apertureLimit, 1);

		const float apertureAngle = (1 - aperture) * CameraParams::apertureAngleScale;
		sincos(apertureAngle, apertureRot.y, apertureRot.x);
	}

	inline void UpdateWhitePoint(in float maxSceneLum, in float exposure, inout float lastSetting)
	{
		const float targetWhitePoint = ldexp(maxSceneLum * exposure, CameraParams::whitePointShift);
		lastSetting = clamp(LuminanceAdaptation(targetWhitePoint, lastSetting), 1, CameraParams::sensorSaturation);
	}

	inline void UpdateCameraSettings(in float avgLogLum, in float maxSceneLum, inout float relativeExposure, inout float whitePoint, out float exposure, out float aperture, out float whitePointFactor, out float2 apertureRot)
	{
		LumAdaptaion::Autoexposure(avgLogLum, relativeExposure, aperture, apertureRot);
		exposure = relativeExposure * CameraParams::normFactor;
		LumAdaptaion::UpdateWhitePoint(maxSceneLum, exposure, whitePoint);
		whitePointFactor = rcp(whitePoint * whitePoint);
	}
}

inline float InvLinearZ(float Z/*from hw Z buffer*/)
{
	return projParams[1] - Z * projParams[2];
}

inline void UpdateFocusSettings(in float aperture, in float scale, in float focusTarget/*hw Z*/, inout float lastSetting, out float2 COCParams)
{
	const float targetSensorPlane = projParams[0]/*F*/ / (1 - projParams[0] * InvLinearZ(focusTarget));
	lastSetting = lerp(targetSensorPlane, lastSetting, camAdaptationFactors[2]);
	COCParams[0] = lastSetting * projParams[2];
	COCParams[1] = lastSetting * (rcp(projParams[0]) - projParams[1]) - 1;
	scale *= aperture * (.5f/*derive sensor size from F*/ * .5f/*CoC diam -> rad*/ / 1.4f/*f-stop for fully opened aperture*/) * projXform[1][1]/*1 / tan(fovy / 2)*/;
	COCParams *= scale;
}

[numthreads(CSConfig::LuminanceReduction::BufferPass::blockSize, 1, 1)]
void main(in uint globalIdx : SV_DispatchThreadID, in uint localIdx : SV_GroupIndex)
{
	// global buffer loading combined with first level reduction
	const float4 batch = asfloat(lumBuffer.Load4(globalIdx * 16));
	const float2 partialReduction = { batch[0] + batch[2], max(batch[1], batch[3]) };

	// bulk of reduction work
	const float2 finalReduction = LumAdaptaion::LocalReduce(partialReduction, localIdx);

	// update camera settings buffer
	if (localIdx == 0)
	{
		float3 lastSettings = asfloat(cameraSettings.Load3(0));
		float exposure, aperture, whitePointFactor;
		float2 apertureRot, COCParams;

		LumAdaptaion::UpdateCameraSettings(finalReduction[0], finalReduction[1], lastSettings[0], lastSettings[1], exposure, aperture, whitePointFactor, apertureRot);

		uint2 resolution;
		uint MSAA;
		ZBuffer.GetDimensions(resolution.x, resolution.y, MSAA);
		UpdateFocusSettings(aperture, resolution.y/*want CoC in fullres pixels*/, ZBuffer[resolution / 2/*focus on center point*/], lastSettings[2], COCParams);

		cameraSettings.Store3(0, asuint(lastSettings));
		cameraSettings.Store3(12, asuint(float3(exposure, aperture, whitePointFactor)));
		cameraSettings.Store4(24, asuint(float4(apertureRot, COCParams)));
	}
}