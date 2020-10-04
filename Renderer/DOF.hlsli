#pragma once

#include "Bokeh.hlsli"	// for area evaluation

// it seems that Kepler doesn't support texture MIN filter\
no way to query support in D3D12?
#define ENABLE_HARDWARE_COC_DOWNSAMPLE 0

namespace DOF
{
	static const float
		layerSeparationCoC = 4 * 2/*to fullres*/,
		layerBlendRangeScale = .4f,
		layerBlendRange = layerSeparationCoC * layerBlendRangeScale;

	enum LayerID
	{
		FOREGROUND_NEAR_LAYER,
		BACKGROUND_NEAR_LAYER,
		FOREGROUND_FAR_LAYER,
		BACKGROUND_FAR_LAYER,
	};

	inline float BlendNear(float CoC)
	{
		return smoothstep(+DOF::layerBlendRange, -DOF::layerBlendRange, CoC - DOF::layerSeparationCoC * sign(CoC));
	}

	// FXC validation errors if place inside 'AreaFactor()', try with newer version
	static const float
		squareCorrection = .9f,	// for opacity boost
		circleFactor = radians(180),
		polyFactor = squareCorrection * Bokeh::areaScale;

	// consider precalculation in camera settings CB during camera setup
	inline float AreaFactor(float aperture)
	{
		// [R..1] -> [0..1]
#if 0
		const float blend = saturate((aperture - Bokeh::R) / (1 - Bokeh::R));
#else
		// mad friendly, rely on constant folding
		const float blend = saturate(aperture / (1 - Bokeh::R) - Bokeh::R / (1 - Bokeh::R));
#endif
		return lerp(polyFactor, circleFactor, blend);
	}

	inline float Opacity(float CoC, float aperture)
	{
		return rcp(AreaFactor(aperture) * CoC * CoC);
	}

	inline float OpacityFullres(float CoC, float aperture)
	{
		return saturate(Opacity(CoC, aperture));
	}

	inline float OpacityHalfres(float CoC, float aperture)
	{
		const float fullresOpacity = Opacity(CoC, aperture);
		const float halfresOpacity = fullresOpacity * 4;
		return saturate(halfresOpacity * (1 - fullresOpacity));
	}

	float HalfresOpacity(float fullresOpacity)
	{
		return saturate(fullresOpacity * 4);
	}

	inline float CoC(float opacity, float aperture)
	{
		return rsqrt(AreaFactor(aperture) * opacity);
	}

#if ENABLE_HARDWARE_COC_DOWNSAMPLE
	inline float DownsampleCoC(uniform Texture2D<float> COCbuffer, uniform SamplerState COCdownsampler, in float2 centerPoint, uniform int2 offset = 0)
	{
		return COCbuffer.SampleLevel(COCdownsampler, centerPoint, 0, offset);
	}
#else
	float DownsampleCoC(uniform Texture2D<float> COCbuffer, uniform SamplerState COCdownsampler, in float2 centerPoint, uniform int2 offset = 0)
	{
		float4 CoCBlock = COCbuffer.Gather(COCdownsampler, centerPoint, offset);
		CoCBlock.xy = min(CoCBlock.xy, CoCBlock.zw);
		return min(CoCBlock.x, CoCBlock.y);
	}
#endif

	struct SplatPoint
	{
		float2	pos : POSITION;
		float2	ext : EXTENTS;
		float2	rot : ROTATION;
		float2	coc : CoC;
		float	apt : APERTURE;
		half4	col : COLOR;
	};
}