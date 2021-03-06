#pragma once

namespace LumAdaptaion
{
	groupshared float2 localData[localDataSize];

	inline void Reduce(inout float2 dst, in float2 src)
	{
		dst[0] += src[0];
		dst[1] = max(dst[1], src[1]);
	}

	inline float2 ReduceSIMD(float2 src)
	{
		return float2(WaveActiveSum(src[0]), WaveActiveMax(src[1]));
	}

	float2 LocalReduce(float2 init, uint localIdx)
	{
		localData[localIdx] = init;
		GroupMemoryBarrierWithGroupSync();

		// inter-warp recursive reduction in shared mem
		uint stride = localDataSize;
		while (stride > WaveGetLaneCount())
		{
			if (localIdx < (stride /= 2u))
			{
				Reduce(localData[localIdx], localData[localIdx + stride]);
				GroupMemoryBarrierWithGroupSync();
			}
		}

		// final intra-warp reduction
		float2 finalReduction;
		if (localIdx < stride)
			finalReduction = ReduceSIMD(localData[localIdx]);
		return finalReduction;
	}
}