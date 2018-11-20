/**
\author		Alexey Shaydurov aka ASH
\date		20.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "tonemapTextureReduction config.hlsli"
#include "luminance.hlsli"

static const uint localDataSize = groupSize * groupSize;
#include "tonemapLocalReduction.hlsli"

Texture2D src : register(t0);
RWByteAddressBuffer dst : register(u1);

// !: no low-level optimizations yet (e.g. GPR pressure)
[numthreads(groupSize, groupSize, 1)]
void main(in uint2 globalIdx : SV_DispatchThreadID, in uint flatLocalIdx : SV_GroupIndex, in uint2 groupIdx : SV_GroupID)
{
	uint2 srcSize;
	src.GetDimensions(srcSize.x, srcSize.y);

	float2 partialReduction = 0;
	const float weight = rcp(srcSize.x * srcSize.y);

	const uint2 dispatchSize = DispatchSize(srcSize), interleaveStride = dispatchSize * blockSize;

	// interleaved tile reduction
	for (uint2 tileCoord = globalIdx * tileSize; tileCoord.y < srcSize.y; tileCoord.y += interleaveStride.y)
		for (tileCoord.x = globalIdx.x * tileSize; tileCoord.x < srcSize.x; tileCoord.x += interleaveStride.x)
			[unroll]
			for (uint2 tileOffset = 0; tileOffset.y < tileSize; tileOffset.y++)
				[unroll]
				for (tileOffset.x = 0; tileOffset.x < tileSize; tileOffset.x++)
				{
					float4 srcPixel = src.Load(uint3(tileCoord, 0), tileOffset);
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

	// bulk of reduction work
	const float2 finalReduction = LocalReduce(partialReduction, flatLocalIdx);

	// store result to global buffer
	const uint flatGroupIdx = groupIdx.y * dispatchSize.x + groupIdx.x;
	if (flatLocalIdx == 0)
		dst.Store2(flatGroupIdx * 8, asuint(finalReduction));
}