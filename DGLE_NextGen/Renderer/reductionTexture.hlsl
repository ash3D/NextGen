/**
\author		Alexey Shaydurov aka ASH
\date		05.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "reductionTexture config.hlsli"
#include "luminance.hlsli"

Texture2D src : register(t0);
RWBuffer<float2> dst : register(u0);

[numthreads(blockSize, blockSize, 1)]
void main(in uint2 globalIdx : SV_DispatchThreadID)
{
	uint2 size;
	src.GetDimensions(size.x, size.y);

	float2 result = 0;
	const float weight = rcp(size.x * size.y);

	[unroll]
	for (uint2 offset = 0; offset.y < reductionFactor; offset.y++)
		[unroll]
		for (; offset.x < reductionFactor; offset.x++)
		{
			const float lum = RGB_2_luminance(src[globalIdx * reductionFactor + offset]);
			// do weighting per-pixel rather than once in final reduction since mul is free here (merged into mad)\
			NOTE: first iteration for unrolled loop can optimize add out thus making mul non-free
			result.x += log(lum + 1) * weight;
			result.y = max(result.y, lum);
		}

    const uint flatIdx = globalIdx.y * (size.x + reductionFactor - 1) / reductionFactor + globalIdx.x;
	dst[flatIdx] = result;
}