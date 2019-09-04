#include "tonemapping config.hlsli"
#include "tonemap params.hlsli"

namespace Tonemapping
{
	static const uint localDataSize = BufferReduction::blockSize;
}
#include "tonemapLocalReduction.hlsli"

#define DXC_NAMESPACE_WORKAROUND 1

#if DXC_NAMESPACE_WORKAROUND
ByteAddressBuffer buffer : register(t1);
RWByteAddressBuffer tonemapParams : register(u2);
#endif

namespace Tonemapping
{
	ByteAddressBuffer buffer : register(t1);
	RWByteAddressBuffer tonemapParams : register(u2);
	cbuffer LumAdaptation : register(b1)
	{
		float lerpFactor;
	}

	inline float LinearizeLum(float src)
	{
		return exp2(src) - 1;
	}

	/*
		similar to Reinhard, maps [0, inf] -> [0, scale]
		!: can produce NaN in corner cases (0, inf, both x and scale)
		alternative formulation can be used: x / (1 + abs(x) / scale), which could be faster (mad + div) and behaves differently in corner cases
	*/
	inline float Compress(float x, uniform float scale)
	{
		return x * scale / (scale + abs(x));
	}

	inline float3 CalcTonemapParams(float2 src, float2 history)
	{
		const float
			sceneKeyValue = LinearizeLum(src[0]),
			targetKeyValue = ldexp(referenceKeyValue, Compress(log2(sceneKeyValue / referenceKeyValue), maxExposureCompensation)),
			targetWorldExposure = targetKeyValue / sceneKeyValue,
			targetNormExposure = targetWorldExposure * normFactor,
			normExposure = clamp(lerp(targetNormExposure, history[0], lerpFactor), exposureLimits[0], exposureLimits[1]),
			targetWhitePoint = src[1] * normExposure,
			whitePoint = clamp(lerp(targetWhitePoint, history[1], lerpFactor), 1, sensorSaturation);
		return float3(normExposure, whitePoint, rcp(whitePoint * whitePoint));
	}
}

[numthreads(Tonemapping::BufferReduction::blockSize, 1, 1)]
void main(in uint globalIdx : SV_DispatchThreadID, in uint localIdx : SV_GroupIndex)
{
	//using namespace Tonemapping;

	// global buffer loading combined with first level reduction
	const float4 batch = asfloat(buffer.Load4(globalIdx * 16));
	const float2 partialReduction = { batch[0] + batch[2], max(batch[1], batch[3]) };

	// bulk of reduction work
	const float2 finalReduction = Tonemapping::LocalReduce(partialReduction, localIdx);

	// update tonemap params buffer
	if (localIdx == 0)
		tonemapParams.Store3(0, asuint(Tonemapping::CalcTonemapParams(finalReduction, asfloat(tonemapParams.Load2(0)))));
}