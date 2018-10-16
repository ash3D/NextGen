/**
\author		Alexey Shaydurov aka ASH
\date		16.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "reductionTexture config.hlsli"
#include "luminance.hlsli"

Texture2D src : register(t0);
RWByteAddressBuffer dst : register(u1);

groupshared float2 localData[groupSize * groupSize];

inline void Reduce(inout float2 dst, in float2 src)
{
	dst[0] += src[0];
	dst[1] = max(dst[1], src[1]);
}

// !: no low-level optimizations yet (e.g. GPR pressure)
[numthreads(groupSize, groupSize, 1)]
void main(in uint2 globalIdx : SV_DispatchThreadID, in uint flatLocalIdx : SV_GroupIndex, in uint2 groupIdx : SV_GroupID)
{
	uint2 srcSize;
	src.GetDimensions(srcSize.x, srcSize.y);

	float2 partialReduction = 0;
	const float weight = rcp(srcSize.x * srcSize.y);

	const uint2 dispatchSize = DispatchSize(srcSize), interleaveStride = dispatchSize * blockSize;

	for (uint2 interleaveOffset = 0; interleaveOffset.y < srcSize.y; interleaveOffset.y += interleaveStride.y)
		for (interleaveOffset.x = 0; interleaveOffset.x < srcSize.x; interleaveOffset.x += interleaveStride.x)
			[unroll]
			for (uint2 tileOffset = 0; tileOffset.y < tileSize; tileOffset.y++)
				[unroll]
				for (tileOffset.x = 0; tileOffset.x < tileSize; tileOffset.x++)
				{
					float4 srcPixel = src[globalIdx * tileSize + interleaveOffset + tileOffset];
					srcPixel.rgb /= srcPixel.a;
					/*
						'max' used to convert NaN to 0
						NaN comes from out-of-bounds pixels in edge tiles - they are which fetched as 0
						and 0/0 produses NaN according to DirectX floating-point rules (https://docs.microsoft.com/en-us/windows/desktop/direct3d11/floating-point-rules)
					*/
					const float lum = max(0, RGB_2_luminance(srcPixel.rgb));
					// do weighting per-pixel rather than once in final reduction since mul is free here (merged into mad)\
					NOTE: first iteration for unrolled loop can optimize add out thus making mul non-free
					partialReduction.x += log2(lum + 1) * weight;
					partialReduction.y = max(partialReduction.y, lum);
				}

	localData[flatLocalIdx] = partialReduction;
	GroupMemoryBarrierWithGroupSync();

	// recursive reduction in shared mem
	for (uint stride = groupSize * groupSize / 2u; stride; stride /= 2u)
	{
		if (flatLocalIdx < stride)
			Reduce(localData[flatLocalIdx], localData[flatLocalIdx + stride]);
		GroupMemoryBarrierWithGroupSync();
	}

	// store result to global buffer
	const uint flatGroupIdx = groupIdx.y * dispatchSize.x + groupIdx.x;
	if (flatLocalIdx == 0)
		dst.Store2(flatGroupIdx * 8, asuint(localData[0]));
}