#include "DOF.hlsli"
#include "Bokeh.hlsli"

namespace DOF
{
	struct SpriteVertex : Bokeh::SpriteVertex
	{
		nointerpolation	float	R : CLIP_CIRCLE_R;
#if !HW_CLIP_DIST_IN_PS
		noperspective	float	apertureEdgeFade0 : EDGE_FADE0;
		noperspective	float	apertureEdgeFade1 : EDGE_FADE1;
		noperspective	float	apertureEdgeFade2 : EDGE_FADE2;
		noperspective	float	apertureEdgeFade3 : EDGE_FADE3;
		noperspective	float	apertureEdgeFade4 : EDGE_FADE4;
#endif
		uint	field			: SV_RenderTargetArrayIndex;
	};
}

class Sprite : BokehSprite
{
	float R, edgeFadeScale;
	uint field;

	DOF::SpriteVertex Corner(uniform uint idx)
	{
		float2 cornerOffset;
#if HW_CLIP_DIST_IN_PS
		DOF::SpriteVertex vert = { BokehSprite::Corner(idx, cornerOffset), R, field };
		vert.apertureCropDist0 *= edgeFadeScale;
		vert.apertureCropDist1 *= edgeFadeScale;
		vert.apertureCropDist2 *= edgeFadeScale;
		vert.apertureCropDist3 *= edgeFadeScale;
		vert.apertureCropDist4 *= edgeFadeScale;
#else
		const Bokeh::SpriteVertex baseVert = BokehSprite::Corner(idx, cornerOffset);
		const DOF::SpriteVertex vert =
		{
			baseVert, R,
			baseVert.apertureCropDist0 * edgeFadeScale,
			baseVert.apertureCropDist1 * edgeFadeScale,
			baseVert.apertureCropDist2 * edgeFadeScale,
			baseVert.apertureCropDist3 * edgeFadeScale,
			baseVert.apertureCropDist4 * edgeFadeScale,
			field
		};
#endif
		return vert;
	}
};

[maxvertexcount(4)]
void main(point DOF::SplatPoint splatPoint[1], inout TriangleStream<DOF::SpriteVertex> spriteCorners)
{
	[branch]
	if (splatPoint[0].coc[1] + .5f > Bokeh::R)
	{
		const Sprite sprite =
		{
			splatPoint[0].pos,
			smoothstep(-DOF::layerBlendRange, +DOF::layerBlendRange, splatPoint[0].coc[0] - DOF::layerSeparationCoC * sign(splatPoint[0].coc[0])),	// blend far
			splatPoint[0].col,
			DOF::OpacityHalfres(splatPoint[0].coc[1], splatPoint[0].apt),
			splatPoint[0].ext,
			(splatPoint[0].coc[1] + .5f) / Bokeh::R/*treat inner R as CoC, blow quad to fit*/,
			splatPoint[0].rot,
			max(splatPoint[0].coc[1] / splatPoint[0].apt + .5f, 1),
			max(splatPoint[0].coc[1] + .5f, 1) / Bokeh::R/*treat inner R as CoC, blow quad to fit*/,
			splatPoint[0].coc[0] > 0
		};

		// expand sprite quad
		spriteCorners.Append(sprite.Corner(0));
		spriteCorners.Append(sprite.Corner(1));
		spriteCorners.Append(sprite.Corner(2));
		spriteCorners.Append(sprite.Corner(3));
	}
}