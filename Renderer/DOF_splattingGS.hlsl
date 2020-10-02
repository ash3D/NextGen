#include "DOF.hlsli"
#include "Bokeh.hlsli"

namespace DOF
{
	struct SpriteVertex : Bokeh::SpriteVertex
	{
		nointerpolation	float	R : CLIP_CIRCLE_R;
		noperspective	float	apertureMainEdgeFade : EDGE_MAIN_FADE;
		noperspective	float	apertureSideEdgesFade[2] : EDGE_SIDE_FADE;
		nointerpolation	uint	field : SV_RenderTargetArrayIndex;
	};
}

class Sprite : BokehSprite
{
	float R, centerEdgeDist, cornerEdgeDist;
	uint field;

	void Tri(uniform uint idx, out DOF::SpriteVertex tri[3])
	{
		tri[0] = Corner(idx, float2(0, cornerEdgeDist));
		tri[1] = Center();
		tri[2] = Corner((idx + 1) % 5, float2(cornerEdgeDist, 0));
	}

	DOF::SpriteVertex Corner(uniform uint idx, in float2 apertureSideEdgesFade)
	{
		const DOF::SpriteVertex vert = { BokehSprite::Corner(idx), R, 0, apertureSideEdgesFade, field };
		return vert;
	}

	DOF::SpriteVertex Center()
	{
		const DOF::SpriteVertex vert = { BokehSprite::Center(), R, centerEdgeDist, centerEdgeDist, centerEdgeDist, field };
		return vert;
	}
};

Sprite MakeSprite(float2 center, half4 color, float2 extents, float aperture, float CoC, float dilatedCoC, float2 rot)
{
	float2 cornerOffsets[5];
	const float centerEdgeDist = max(dilatedCoC + .5f, 1)/*in pixel units*/;
#if DXC_BASE_WORKAROUND
	const BokehSprite base = Bokeh::MakeSprite(float3(center, DOF::BlendNear(CoC)), color, extents * sign(CoC)/*mirror bokeh for foreground field*/,
		(dilatedCoC + .5f) / Bokeh::R/*treat inner R as CoC, blow quad to fit*/, rot, cornerOffsets);
#endif
	const Sprite sprite =
	{
#if DXC_BASE_WORKAROUND
		base.corners, base.center, base.color, base.circleScale,
#else
		Bokeh::MakeSprite(float3(center, DOF::BlendNear(CoC)), color, extents * sign(CoC)/*mirror bokeh for foreground field*/,
			(dilatedCoC + .5f) / Bokeh::R/*treat inner R as CoC, blow quad to fit*/, rot, cornerOffsets),
#endif
		max(dilatedCoC / aperture + .5f, 1),
		centerEdgeDist,
		centerEdgeDist * (Bokeh::cornerBladeDist / Bokeh::R/*'centerEdgeDist / Bokeh::R' rescale inner R -> outer R aka quad R, regroup into () for constant folding*/),
		CoC > 0
	};
	return sprite;
}

void EmitTri(in DOF::SpriteVertex tri[3], inout TriangleStream<DOF::SpriteVertex> spriteVerts)
{
	spriteVerts.Append(tri[0]);
	spriteVerts.Append(tri[1]);
	spriteVerts.Append(tri[2]);
}

[maxvertexcount(Bokeh::vcount)]
void main(point DOF::SplatPoint splatPoint[1], inout TriangleStream<DOF::SpriteVertex> spriteVerts)
{
	[branch]
	if (splatPoint[0].col.a)
	{
		const Sprite sprite = MakeSprite(splatPoint[0].pos, splatPoint[0].col, splatPoint[0].ext, splatPoint[0].apt, splatPoint[0].coc[0], splatPoint[0].coc[1], splatPoint[0].rot);

		// expand sprite
		DOF::SpriteVertex tri[3];

		sprite.Tri(0, tri);
		EmitTri(tri, spriteVerts);
		spriteVerts.RestartStrip();

		sprite.Tri(1, tri);
		EmitTri(tri, spriteVerts);
		spriteVerts.RestartStrip();

		sprite.Tri(2, tri);
		EmitTri(tri, spriteVerts);
		spriteVerts.RestartStrip();

		sprite.Tri(3, tri);
		EmitTri(tri, spriteVerts);
		spriteVerts.RestartStrip();

		sprite.Tri(4, tri);
		EmitTri(tri, spriteVerts);
	}
}