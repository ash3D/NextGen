#pragma once

// 9-tap filter
float3 UpsampleBlur(uniform Texture2D src, uniform SamplerState tapFilter, in float2 center, in uint lod)
{
	float3 acc = src.SampleLevel(tapFilter, center, lod) * .25f;

	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, -1)) * .0625f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, -1)) * .0625f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, +1)) * .0625f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, +1)) * .0625f;

	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, 0)) * .125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, 0)) * .125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(0, -1)) * .125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(0, +1)) * .125f;

	return acc;
}