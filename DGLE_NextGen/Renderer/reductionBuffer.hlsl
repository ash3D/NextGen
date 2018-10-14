/**
\author		Alexey Shaydurov aka ASH
\date		15.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "reductionBuffer config.hlsli"

RWByteAddressBuffer buffer : register(u0);

groupshared float2 localData[blockSize];

inline float2 Reduce(float2 a, float2 b)
{
	return float2(a[0] + b[0], max(a[1], b[1]));
}

inline float LinearizeLum(float src)
{
	return exp2(src) - 1;
}

inline float2 CalcTonemapParams(float2 src)
{
	static const float keyValue = LinearizeLum(.5f);
	const float middleGray = LinearizeLum(src[0]), exposure = keyValue / middleGray, whitePoint = src[1] * exposure;
	return float2(exposure, rcp(whitePoint * whitePoint));
}

[numthreads(blockSize, 1, 1)]
void main(in uint globalIdx : SV_DispatchThreadID, in uint localIdx : SV_GroupIndex, in uint blockIdx : SV_GroupID)
{
	// global buffer loading combined with first level reduction
	uint bufferSize;
	buffer.GetDimensions(bufferSize);
    localData[localIdx] = Reduce(asfloat(buffer.Load2(globalIdx * 8)), asfloat(buffer.Load2((globalIdx + blockSize) * 8)));
	GroupMemoryBarrierWithGroupSync();

	// recursive reduction in shared mem
	for (uint stride = blockSize / 2u; stride; stride /= 2u)
	{
		if (localIdx < stride)
			localData[localIdx] = Reduce(localData[localIdx], localData[localIdx + stride]);
		GroupMemoryBarrierWithGroupSync();
	}

	// store result to global buffer
	if (localIdx == 0)
        buffer.Store2(0, asuint(CalcTonemapParams(localData[0])));
}