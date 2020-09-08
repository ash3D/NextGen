#pragma once

#include "Bokeh.hlsli"	// for R

// return 0 permanently in PS on Kepler, try with other GPUs
#define HW_CLIP_DIST_IN_PS 0

namespace DOF
{
	static const float
		layerSeparationCoC = 4,
		layerBlendRangeScale = .4f,
		layerBlendRange = layerSeparationCoC * layerBlendRangeScale;

	enum LayerID
	{
		FOREGROUND_NEAR_LAYER,
		BACKGROUND_NEAR_LAYER,
		FOREGROUND_FAR_LAYER,
		BACKGROUND_FAR_LAYER,
	};

	// FXC validation errors if place inside 'Opacity()', try with newer version
	static const float
		squareCorrection = .9f,	// for opacity boost
		circleFactor = radians(180),
		polyFactor = squareCorrection * tan(radians(36)) * 5;

	inline float Opacity(float CoC, float aperture)
	{
		// [R..1] -> [0..1]
#if 0
		const float blend = saturate((aperture - Bokeh::R) / (1 - Bokeh::R));
#else
		// mad friendly, rely on constant folding
		const float blend = saturate(aperture / (1 - Bokeh::R) - Bokeh::R / (1 - Bokeh::R));
#endif
		const float factor = lerp(polyFactor, circleFactor, blend);
		return rcp(factor * CoC * CoC);
	}

	inline float OpacityHalfres(float CoC, float aperture)
	{
		const float halfresOpacity = Opacity(CoC, aperture);
		const float fullresOpacity = halfresOpacity * .25f;
		return saturate(halfresOpacity * (1 - fullresOpacity));
	}

#if 0
	// DXC crash
#define GENERATE_COC_SELECTOR(dim)													\
	inline void SelectCoC(inout vector<float, dim> dst, in vector<float, dim> src)	\
	{																				\
		dst = min(dst, src);														\
	}

	GENERATE_COC_SELECTOR(1)
	GENERATE_COC_SELECTOR(4)
#else
#define GENERATE_COC_SELECTOR(T)													\
	inline void SelectCoC(inout T dst, in T src)									\
	{																				\
		dst = min(dst, src);														\
	}

	GENERATE_COC_SELECTOR(float)
	GENERATE_COC_SELECTOR(float4)
#endif
#undef GENERATE_COC_SELECTOR

#if 1
	inline float SelectCoC(float4 CoCs)
	{
		CoCs.xy = min(CoCs.xy, CoCs.zw);
		return min(CoCs.x, CoCs.y);
	}
#elif 1
	inline float SelectCoC(float4 CoCs)
	{
		CoCs.xy = min(abs(CoCs.xy), abs(CoCs.zw)) * max(sign(CoCs.xy), sign(CoCs.zw));
		return min(abs(CoCs.x), abs(CoCs.y)) * max(sign(CoCs.x), sign(CoCs.y));
	}
#else
	inline float SelectCoC(float4 CoCs)
	{
		CoCs.xy = abs(CoCs.xy) <= abs(CoCs.zw) ? CoCs.xy : CoCs.zw;
		return abs(CoCs.x) <= abs(CoCs.y) ? CoCs.x : CoCs.y;
	}
#endif

	struct SplatPoint
	{
		float2	pos : POSITION;
		float2	ext : EXTENTS;
		float2	rot : ROTATION;
		float2	coc : CoC;
		float	apt : APERTURE;
		half3	col : COLOR;
	};
}