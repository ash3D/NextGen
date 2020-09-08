#include "DOF.hlsli"

SamplerState COCdownsampleFilter : register(s0);
Texture2D<float2> COCbuffer : register(t4);
Texture2D src : register(t5);

struct Layers
{
	half4 near : SV_Target0, far : SV_Target1;
};

Layers main(nointerpolation half4 spriteColor : COLOR, noperspective float2 circleDir : CLIP_CIRCLE_DIR, noperspective float4 pos/*.z is blend far*/ : SV_Position,
	noperspective float apertureCropDist0 : SV_ClipDistance0,
	noperspective float apertureCropDist1 : SV_ClipDistance1,
	noperspective float apertureCropDist2 : SV_ClipDistance2,
	noperspective float apertureCropDist3 : SV_ClipDistance3,
	noperspective float apertureCropDist4 : SV_ClipDistance4,
	nointerpolation float R : CLIP_CIRCLE_R
#if !HW_CLIP_DIST_IN_PS
	, noperspective float apertureEdgeFade0 : EDGE_FADE0
	, noperspective float apertureEdgeFade1 : EDGE_FADE1
	, noperspective float apertureEdgeFade2 : EDGE_FADE2
	, noperspective float apertureEdgeFade3 : EDGE_FADE3
	, noperspective float apertureEdgeFade4 : EDGE_FADE4
#endif
)
{
	const float circleDist = R - length(circleDir);
	clip(circleDist);

	float edgeFade = 1;
#if HW_CLIP_DIST_IN_PS
	edgeFade *= saturate(apertureCropDist0);
	edgeFade *= saturate(apertureCropDist1);
	edgeFade *= saturate(apertureCropDist2);
	edgeFade *= saturate(apertureCropDist3);
	edgeFade *= saturate(apertureCropDist4);
#else
	edgeFade *= saturate(apertureEdgeFade0);
	edgeFade *= saturate(apertureEdgeFade1);
	edgeFade *= saturate(apertureEdgeFade2);
	edgeFade *= saturate(apertureEdgeFade3);
	edgeFade *= saturate(apertureEdgeFade4);
#endif

	const float circleFade = saturate(circleDist);
	spriteColor.a *= min(edgeFade, circleFade);

	Layers layers = { spriteColor, spriteColor };
	layers.near.a *= 1 - pos.z;
	layers.far.a *= pos.z;
	return layers;
}