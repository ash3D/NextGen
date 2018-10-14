/**
\author		Alexey Shaydurov aka ASH
\date		15.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "reductionTexture config.hlsli"
#include "luminance.hlsli"

Texture2D src : register(t0);
RWByteAddressBuffer dst : register(u0);

groupshared float2 localData[groupSize * groupSize];

inline void Reduce(inout float2 dst, in float2 src)
{
	dst[0] += src[0];
	dst[1] = max(dst[1], src[1]);
}

[numthreads(groupSize, groupSize, 1)]
void main(in uint2 globalIdx : SV_DispatchThreadID, in uint flatLocalIdx : SV_GroupIndex, in uint2 groupIdx : SV_GroupID)
{
	uint2 srcSize;
	src.GetDimensions(srcSize.x, srcSize.y);

	float2 partialRedution = 0;
	const float weight = rcp(srcSize.x * srcSize.y);

	const uint2 interleaveStride = DispatchSize(srcSize) * blockSize;

	for (uint2 interleaveOffset = 0; interleaveOffset.y < srcSize.y; interleaveOffset += interleaveStride.y)
		for (; interleaveOffset.x < srcSize.x; interleaveOffset.x += interleaveStride.x)
			[unroll]
			for (uint2 tileOffset = 0; tileOffset.y < tileSize; tileOffset.y++)
				[unroll]
				for (; tileOffset.x < tileSize; tileOffset.x++)
				{
					const float lum = RGB_2_luminance(src[globalIdx * tileSize + interleaveOffset + tileOffset]);
					// do weighting per-pixel rather than once in final reduction since mul is free here (merged into mad)\
					NOTE: first iteration for unrolled loop can optimize add out thus making mul non-free
					partialRedution.x += log2(lum + 1) * weight;
					partialRedution.y = max(partialRedution.y, lum);
				}

	localData[flatLocalIdx] = partialRedution;
	GroupMemoryBarrierWithGroupSync();

	// recursive reduction in shared mem
	for (uint stride = groupSize * groupSize / 2u; stride; stride /= 2u)
	{
		if (flatLocalIdx < stride)
			Reduce(localData[flatLocalIdx], localData[flatLocalIdx + stride]);
		GroupMemoryBarrierWithGroupSync();
	}

	// store result to global buffer
	const uint flatGroupIdx = groupIdx.y * groupSize + groupIdx.x;
	if (flatLocalIdx == 0)
        dst.Store2(flatGroupIdx * 8, asuint(localData[0]));
}