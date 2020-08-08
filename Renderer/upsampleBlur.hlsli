#pragma once

#define TEX_SELECT_0 Texture2D
#define TEX_SELECT_1 Texture2DArray

// 9-tap filter
// want templates in HLSL!
#define GENERATE_UPSAMPLE_BLUR(dim, array)																													\
vector<float, dim> UpsampleBlur##dim(uniform TEX_SELECT_##array src, uniform SamplerState tapFilter, in vector<float, 2 + array> center, in uint lod = 0)	\
{																																							\
	vector<float, dim> acc = src.SampleLevel(tapFilter, center, lod) * .25f;																				\
																																							\
	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, -1)) * .0625f;																					\
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, -1)) * .0625f;																					\
	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, +1)) * .0625f;																					\
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, +1)) * .0625f;																					\
																																							\
	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, 0)) * .125f;																					\
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, 0)) * .125f;																					\
	acc += src.SampleLevel(tapFilter, center, lod, int2(0, -1)) * .125f;																					\
	acc += src.SampleLevel(tapFilter, center, lod, int2(0, +1)) * .125f;																					\
																																							\
	return acc;																																				\
}

GENERATE_UPSAMPLE_BLUR(3, 0)
GENERATE_UPSAMPLE_BLUR(4, 1)
#undef TEX_SELECT_0
#undef TEX_SELECT_1
#undef GENERATE_UPSAMPLE_BLUR

float UpsampleBlurA(uniform Texture2D src, uniform SamplerState tapFilter, in float2 center, in uint lod = 0)
{
	float acc = src.SampleLevel(tapFilter, center, lod).a * .25f;

	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, -1)).a * .0625f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, -1)).a * .0625f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, +1)).a * .0625f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, +1)).a * .0625f;

	acc += src.SampleLevel(tapFilter, center, lod, int2(-1, 0)).a * .125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(+1, 0)).a * .125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(0, -1)).a * .125f;
	acc += src.SampleLevel(tapFilter, center, lod, int2(0, +1)).a * .125f;

	return acc;
}