#include "lensFlare.hlsli"
#include "Bokeh.hlsli"
#include "luminance.hlsli"

struct Flare
{
	float	pos, clipDist, clipRate, size;
	float3	tint;
};

inline Flare NormalizedFlare(uniform float pos, uniform float clipDist, uniform float clipRate, uniform float size, uniform float3 tint)
{
	const Flare flare =
	{
		pos,
		clipDist,
		clipRate,
		size,
		tint * (LensFlare::strength / LensFlare::normRebalance / (size * size))
	};
	return flare;
}

static const uint spriteCount = 16;
static const Flare flares[spriteCount] =
{
	NormalizedFlare(+.75f, .95f, +2, 02e-2f, float3(1, .1f, .2f)),
	NormalizedFlare(+.65f, .96f, +4, 36e-2f, float3(.32972906104158754554855614264854f, .61868584833867154071446877275424f, .02775528546756561097985170784822f)/*Yellow Green*/),
	NormalizedFlare(+.55f, .99f, +2, 60e-2f, float3(.97165590238637456471396214447593f, .53223634885734243353701339994201f, .53223634885734243353701339994201f)/* x 1.9*/),
	NormalizedFlare(+.45f, .97f, +2, 14e-2f, float3(.246800448619796293314057045002f, .62534484128146821951929883606923f, .9573697075429787247921447109925f)/*Light Sky Blue*/),
	NormalizedFlare(+.40f, .91f, -5, 26e-2f, float3(.51139784336124977090208533919786f, .28012439413544338607211231575895f, .28012439413544338607211231575895f)),
	NormalizedFlare(+.35f, .83f, -3, 02e-2f, float3(.42590531257118362471089082975699f, .69408064322754064880929831324786f, .79691667058787318936446163616423f)/*Light Blue*/),
	NormalizedFlare(+.10f, .80f, -4, 03e-2f, float3(.25902736135426154203530162104999f, .01991783974317256116706142173208f, .76674374285239248160424050638415f)/*Blue Violet*/),
	NormalizedFlare(+.05f, .79f, +5, 04e-2f, float3(.2f, .4f, .9f)),
	NormalizedFlare(-.05f, .75f, +6, 03e-2f, float3(.1f, 1, .2f)),
	NormalizedFlare(-.25f, .92f, -6, 06e-2f, float3(1, .14198031732529604546725838406154f, .46474115968262690459304099582766f)/*Hot Pink*/),
	NormalizedFlare(-.30f, .91f, -4, 04e-2f, float3(.00902149327739830329462161664214f, .28445208998152706802447756529981f, 1)/*Dodger Blue*/),
	NormalizedFlare(-.50f, .96f, +6, 12e-2f, float3(.44232297499919180243000367717245f, .56049926934865771176795532017781f, .73720491349236521182991852320236f)/*Light Steel Blue*/),
	NormalizedFlare(-.55f, .94f, +9, 92e-2f, float3(.04777575977224431334356961216502f, .75189520571652404075405019036969f, .6387795770956960886417041271288f)/*Turquoise*/),
	NormalizedFlare(-.70f, .91f, -7, 50e-2f, float3(.65237022781409838656258648254591f, .46474115968262690459304099582766f, .26735808927805888107554077895316f)/*Tan*/),
	NormalizedFlare(-.75f, .92f, +8, 80e-2f, float3(0, .52952324599076426653845702674723f, 1)/*Deep Sky Blue*/),
	NormalizedFlare(-1.2f, .96f, +5, 08e-2f, float3(.71546555620858302825678206624135f, .16364069912887604055713628938231f, .29765266858042379221938470586868f)/*Pale Violet Red*/)
};

float3 MaxNormalizedTint()
{
	float3 maxTint = 0;
	for (uint lenseID = 0; lenseID < spriteCount; lenseID++)
		maxTint = max(maxTint, flares[lenseID].tint);
	return maxTint;
}

static const float3 maxNormalizedTint = MaxNormalizedTint();

namespace LensFlare
{
	struct SpriteVertex : Bokeh::SpriteVertex
	{
		noperspective float edgeClipDist : SV_ClipDistance;
	};
}

class Sprite : BokehSprite
{
	float cornerClipDists[5], centerClipDist;

	void Tri(uniform uint idx, out LensFlare::SpriteVertex tri[3])
	{
		tri[0] = Corner(idx);
		tri[1] = Center();
		tri[2] = Corner((idx + 1) % 5);
	}

	LensFlare::SpriteVertex Corner(uniform uint idx)
	{
		const LensFlare::SpriteVertex vert =
		{
			BokehSprite::Corner(idx),
			cornerClipDists[idx]
		};
		return vert;
	}

	LensFlare::SpriteVertex Center()
	{
		const LensFlare::SpriteVertex vert =
		{
			BokehSprite::Center(),
			centerClipDist
		};
		return vert;
	}
};

float CornerClipDist(float2 cornerOffset, float3 edgeClip)
{
	return dot(edgeClip.xy, cornerOffset) + edgeClip.z;
}

Sprite MakeSprite(float2 center, half4 color, float2 extents, float aperture, float2 rot, float3 edgeClip)
{
	float2 cornerOffsets[5];
#if DXC_BASE_WORKAROUND
	const BokehSprite base = Bokeh::MakeSprite(float3(center, 0), color, extents, aperture / Bokeh::R, rot, cornerOffsets);
#endif
	const Sprite sprite =
	{
#if DXC_BASE_WORKAROUND
		base.corners, base.center, base.color, base.circleScale,
#else
		Bokeh::MakeSprite(float3(center, 0), color, extents, aperture / Bokeh::R, rot, cornerOffsets),
#endif
		CornerClipDist(cornerOffsets[0], edgeClip),
		CornerClipDist(cornerOffsets[1], edgeClip),
		CornerClipDist(cornerOffsets[2], edgeClip),
		CornerClipDist(cornerOffsets[3], edgeClip),
		CornerClipDist(cornerOffsets[4], edgeClip),
		edgeClip.z	// center offset == 0 => dot part == 0
	};
	return sprite;
}

void EmitTri(in LensFlare::SpriteVertex tri[3], inout TriangleStream<LensFlare::SpriteVertex> flareSpriteVerts)
{
	flareSpriteVerts.Append(tri[0]);
	flareSpriteVerts.Append(tri[1]);
	flareSpriteVerts.Append(tri[2]);
}

[maxvertexcount(Bokeh::vcount)]
[instance(spriteCount)]
void main(point LensFlare::Source flareSource[1], in uint lenseID : SV_GSInstanceID, inout TriangleStream<LensFlare::SpriteVertex> flareSpriteVerts)
{
	const float apertureExposure = flareSource[0].ext.y * flareSource[0].ext.y;
	const float lumThreshold = LensFlare::threshold * apertureExposure/*cancel out aperture contribution to exposure as it affects flare area but not intensity*/;

	// cull phase 1 - faint source
	[branch]
	if (RGB_2_luminance(flareSource[0].col.rgb * maxNormalizedTint) >= lumThreshold)
	{
		float4 color = flareSource[0].col;
		color.rgb *= flares[lenseID].tint;
		const float lum = RGB_2_luminance(color);

		// cull phase 2 - faint flares
		[branch]
		if (lum >= lumThreshold)
		{
			// smooth fadeout for culled sprites
			color.a *= smoothstep(lumThreshold, lumThreshold * LensFlare::fadeoutRange, lum);

			float3 edgeClip = flareSource[0].edg;
			edgeClip.xy *= sign(flares[lenseID].clipRate);
			edgeClip.z *= flares[lenseID].clipDist - edgeClip.z;
			edgeClip.z *= abs(flares[lenseID].clipRate);

			const Sprite sprite = MakeSprite(flareSource[0].pos * flares[lenseID].pos, color, flareSource[0].ext * flares[lenseID].size,
				flareSource[0].ext.y/*ext.y holds unmodified aperture*/, flareSource[0].rot, edgeClip);

			// expand sprite

			LensFlare::SpriteVertex tri[3];

			sprite.Tri(0, tri);
			EmitTri(tri, flareSpriteVerts);
			flareSpriteVerts.RestartStrip();

			sprite.Tri(1, tri);
			EmitTri(tri, flareSpriteVerts);
			flareSpriteVerts.RestartStrip();

			sprite.Tri(2, tri);
			EmitTri(tri, flareSpriteVerts);
			flareSpriteVerts.RestartStrip();

			sprite.Tri(3, tri);
			EmitTri(tri, flareSpriteVerts);
			flareSpriteVerts.RestartStrip();

			sprite.Tri(4, tri);
			EmitTri(tri, flareSpriteVerts);
		}
	}
}