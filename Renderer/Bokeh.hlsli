#pragma once

#define DXC_NAMESPACE_WORKAROUND 1
#define DXC_BASE_WORKAROUND 1

namespace Bokeh
{
	static const uint vcount = 5 * 3;

	struct SpriteVertex
	{
		nointerpolation	half4	col	: COLOR;
		noperspective	float2	dir	: CLIP_CIRCLE_DIR;	// scaled dir to entrance lens edge
		noperspective	float4	pos	: SV_Position;
	};

	// inner radius
	static const float R = cos(radians(36));

	static const float edge = 2 * sin(radians(36));

	// dist from corner to neighbor blade
	static const float cornerBladeDist = edge * sin(radians(180 - (180 - 90 - 36) * 2))/*project onto edge normal*/;

	// unit polygon`s area, scaled with R as 'area = areaScale * R * R'
	static const float areaScale = tan(radians(36)) * 5;

	inline float2 CornerOffset(uniform float angle/*CW*/)
	{
		float2 offset;
		sincos(radians(angle), offset.x, offset.y);
		return offset;
	}

#if !DXC_NAMESPACE_WORKAROUND
	static const float2 cornersLUT[5] =
	{
		CornerOffset(0),
		CornerOffset(72),
		CornerOffset(144),
		CornerOffset(216),
		CornerOffset(288)
	};
#endif
}

#if DXC_NAMESPACE_WORKAROUND
static const float2 cornersLUT[5] =
{
	Bokeh::CornerOffset(0),
	Bokeh::CornerOffset(72),
	Bokeh::CornerOffset(144),
	Bokeh::CornerOffset(216),
	Bokeh::CornerOffset(288)
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
		float2	corners[5];
		float3	center;
		half4	color;
		float	circleScale;

		Bokeh::SpriteVertex Corner(uniform uint idx)
		{
			const Bokeh::SpriteVertex vert =
			{
				color,
				cornersLUT[idx] * circleScale,
				float4(corners[idx], center.z, 1)
			};
			return vert;
		}

		Bokeh::SpriteVertex Center()
		{
			const Bokeh::SpriteVertex vert =
			{
				color,
				(float2)0,
				float4(center, 1)
			};
			return vert;
		}
	};
#if !DXC_NAMESPACE_WORKAROUND
}
#endif

namespace Bokeh
{
	float2 SpriteCorner(uniform uint idx, in float2 center, in float2 extents, in float2x2 rot, out float2 cornerOffset)
	{
		cornerOffset = mul(cornersLUT[idx], rot);
		return center + extents * cornerOffset;
	}

	BokehSprite MakeSprite(in float3 center, in half4 color, in float2 extents, in float circleScale, in float2 rot, out float2 cornerOffsets[5])
	{
		const float2x2 rotMatrix = float2x2(rot.xy, -rot.y, rot.x);
		const BokehSprite sprite =
		{
			SpriteCorner(0, center, extents, rotMatrix, cornerOffsets[0]),
			SpriteCorner(1, center, extents, rotMatrix, cornerOffsets[1]),
			SpriteCorner(2, center, extents, rotMatrix, cornerOffsets[2]),
			SpriteCorner(3, center, extents, rotMatrix, cornerOffsets[3]),
			SpriteCorner(4, center, extents, rotMatrix, cornerOffsets[4]),
			center,
			color,
			circleScale
		};
		return sprite;
	}
}