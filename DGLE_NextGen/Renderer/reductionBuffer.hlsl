/**
\author		Alexey Shaydurov aka ASH
\date		05.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "reductionBuffer config.hlsli"

RWBuffer<float2> buffer : register(u0);

groupshared float2 localData[blockSize];

inline float2 Reduce(float2 a, float2 b)
{
	return float2(a[0] + b[0], max(a[1], b[1]));
}

inline float2 CalcTonemapParams(float2 src)
{
	return float2(exp(src[0]) - 1, rcp(src[1] * src[1]));
}

[numthreads(blockSize, 1, 1)]
void main(in uint globalIdx : SV_DispatchThreadID, in uint localIdx : SV_GroupIndex, in uint blockIdx : SV_GroupID, uniform bool final)
{
	// global buffer loading combined with first level reduction
	uint bufferSize;
	buffer.GetDimensions(bufferSize);
	const uint blockCount = (bufferSize + blockSize - 1u) / blockSize;
	localData[localIdx] = Reduce(buffer[globalIdx], buffer[globalIdx + (blockCount + 1u) / 2u * blockSize]);
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
	{
		if (final)
			buffer[0] = CalcTonemapParams(localData[0]);
		else
			buffer[blockIdx] = localData[0];
	}
}