#pragma once

#define DXC_NAMESPACE_WORKAROUND 1

namespace Bokeh
{
	struct SpriteVertex
	{
		nointerpolation	half4	col					: COLOR;
		noperspective	float2	dir					: CLIP_CIRCLE_DIR;	// scaled dir to entrance lens edge
		noperspective	float4	pos					: SV_Position;
		noperspective	float	apertureCropDist0	: SV_ClipDistance0;
		noperspective	float	apertureCropDist1	: SV_ClipDistance1;
		noperspective	float	apertureCropDist2	: SV_ClipDistance2;
		noperspective	float	apertureCropDist3	: SV_ClipDistance3;
		noperspective	float	apertureCropDist4	: SV_ClipDistance4;
	};

	// inner radius
	static const float R = cos(radians(36));

	inline float2 N(uniform float a)
	{
		float2 n;
		sincos(radians(a), n.x, n.y);
		return n;
	}

	inline float BladeDist(uniform float2 cornerOffset, uniform float bladeAngle)
	{
		return dot(cornerOffset, N(bladeAngle)) + R;
	}

	struct SpriteCornerDesc
	{
		float2	offset;
		float	apertureCropDist[5];
	};

	inline SpriteCornerDesc GenerateSpriteCornerDesc(uniform float2 offset)
	{
		const SpriteCornerDesc corner =
		{
			offset,
			BladeDist(offset, 0),
			BladeDist(offset, +72),
			BladeDist(offset, -72),
			BladeDist(offset, +144),
			BladeDist(offset, -144)
		};
		return corner;
	}

#if !DXC_NAMESPACE_WORKAROUND
	static const SpriteCornerDesc cornersLUT[3] =
	{
		GenerateSpriteCornerDesc(float2(-1, +1)),
		GenerateSpriteCornerDesc(float2(+2, +1)),
		GenerateSpriteCornerDesc(float2(-1, -2))
	};
#endif
}

#if DXC_NAMESPACE_WORKAROUND
static const Bokeh::SpriteCornerDesc cornersLUT[3] =
{
	Bokeh::GenerateSpriteCornerDesc(float2(-1, +1)),
	Bokeh::GenerateSpriteCornerDesc(float2(+2, +1)),
	Bokeh::GenerateSpriteCornerDesc(float2(-1, -2))
};
#endif

#if DXC_NAMESPACE_WORKAROUND
	class BokehSprite
#else
namespace Bokeh
{
	class Sprite
#endif
	{
		float3	center;
		half4	color;
		float2	extents;
		float	circleScale;
		float2	rot;

		Bokeh::SpriteVertex Corner(uniform uint idx, out float2 cornerOffset)
		{
			const float2x2 rotMatrix = float2x2(rot.xy, -rot.y, rot.x);
			cornerOffset = mul(cornersLUT[idx].offset, rotMatrix);
			const Bokeh::SpriteVertex vert =
			{
				color,
				cornersLUT[idx].offset * circleScale,
				float4(center + extents * cornerOffset, center.z, 1),
				cornersLUT[idx].apertureCropDist
			};
			return vert;
		}
	};
#if !DXC_NAMESPACE_WORKAROUND
}
#endif