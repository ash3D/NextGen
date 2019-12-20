#include "lensFlare.hlsli"

struct Flare
{
	float	pos, clipDist, clipSpeed, size;
	float3	tint;
};

inline Flare NormalizedFlare(uniform float pos, uniform float clipDist, uniform float clipSpeed, uniform float size, uniform float3 tint)
{
	const Flare flare =
	{
		pos,
		clipDist,
		clipSpeed,
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

struct SpriteVertex
{
	nointerpolation	half4	col					: COLOR;
	noperspective	float4	pos					: SV_Position;
	noperspective	float	apertureCropDist0	: SV_ClipDistance0;
	noperspective	float	apertureCropDist1	: SV_ClipDistance1;
	noperspective	float	apertureCropDist2	: SV_ClipDistance2;
	noperspective	float	apertureCropDist3	: SV_ClipDistance3;
	noperspective	float	apertureCropDist4	: SV_ClipDistance4;
	noperspective	float	edgeClipDist		: SV_ClipDistance5;
};

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

static const SpriteCornerDesc cornersLUT[4] =
{
	GenerateSpriteCornerDesc(float2(-1, +1)),
	GenerateSpriteCornerDesc(float2(+1, +1)),
	GenerateSpriteCornerDesc(float2(-1, -1)),
	GenerateSpriteCornerDesc(float2(+1, -1))
};

class Sprite
{
	float2	center;
	half4	color;
	float2	extents;
	float2	rot;
	float3	edgeClip;

	SpriteVertex Corner(uniform uint idx)
	{
		const float2x2 rotMatrix = float2x2(rot.xy, -rot.y, rot.x);
		const float2 cornerOffset = mul(cornersLUT[idx].offset, rotMatrix);
		const SpriteVertex vert =
		{
			color,
			float4(center + extents * cornerOffset, 0, 1),
			cornersLUT[idx].apertureCropDist,
			dot(edgeClip.xy, cornerOffset) + edgeClip.z
		};
		return vert;
	}
};

[maxvertexcount(4)]
[instance(spriteCount)]
void main(point LensFlare::Source flareSource[1], in uint lenseID : SV_GSInstanceID, inout TriangleStream<SpriteVertex> flareSpriteCorners)
{
	// cull
	[branch]
	if (flareSource[0].col.a)
	{
		Sprite sprite =
		{
			flareSource[0].pos,
			flareSource[0].col,
			flareSource[0].ext,
			flareSource[0].rot,
			flareSource[0].edg
		};
		sprite.center *= flares[lenseID].pos;
		sprite.color.rgb *= flares[lenseID].tint;
		sprite.extents *= flares[lenseID].size;
		sprite.edgeClip.xy *= sign(flares[lenseID].clipSpeed);
		sprite.edgeClip.z *= flares[lenseID].clipDist - sprite.edgeClip.z;
		sprite.edgeClip.z *= abs(flares[lenseID].clipSpeed);

		// expand sprite quad
		flareSpriteCorners.Append(sprite.Corner(0));
		flareSpriteCorners.Append(sprite.Corner(1));
		flareSpriteCorners.Append(sprite.Corner(2));
		flareSpriteCorners.Append(sprite.Corner(3));
	}
}