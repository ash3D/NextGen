#include "DOF.hlsli"
#include "Bokeh.hlsli"	// temp for R

SamplerState COCdownsampleFilter : register(s0);
Texture2D<float2> COCbuffer : register(t4);
Texture2D src : register(t5);

struct Layers
{
	half4 near : SV_Target0, far : SV_Target1;
};

Layers main(nointerpolation half4 spriteColor : COLOR, noperspective float2 circleDir : CLIP_CIRCLE_DIR, noperspective float4 pos : SV_Position,
	noperspective float apertureCropDist0 : SV_ClipDistance0,
	noperspective float apertureCropDist1 : SV_ClipDistance1,
	noperspective float apertureCropDist2 : SV_ClipDistance2,
	noperspective float apertureCropDist3 : SV_ClipDistance3,
	noperspective float apertureCropDist4 : SV_ClipDistance4,
	nointerpolation float R : CLIP_CIRCLE_R/*, nointerpolation float edgeFadeScale : FADE_SCALE*/
#if !HW_CLIP_DIST_IN_PS
	, noperspective float apertureEdgeFade0 : EDGE_FADE0
	, noperspective float apertureEdgeFade1 : EDGE_FADE1
	, noperspective float apertureEdgeFade2 : EDGE_FADE2
	, noperspective float apertureEdgeFade3 : EDGE_FADE3
	, noperspective float apertureEdgeFade4 : EDGE_FADE4
#endif
)
{
	//const float R2 = dot(circleDir, circleDir);
	//clip(1 - R2);
	const float circleDist = R - length(circleDir);
	clip(circleDist);
	//const float2 edgeFade = saturate((1 - abs(circleDir)) * max(abs(pos.z)/* * 2*/, 1));
	//const float2 edgeFade = saturate((1 - abs(circleDir)) * edgeFadeScale);
	//spriteColor.a *= edgeFade.x;
	//spriteColor.a *= edgeFade.y;
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

#if 0
	const float pixelCoC = src[pos.xy].a;
#else
	float2 srcSize, fullres;
	src.GetDimensions(srcSize.x, srcSize.y);
	COCbuffer.GetDimensions(fullres.x, fullres.y);
	const float2 center = pos.xy / srcSize;
	const float2 centerPoint = floor(center * fullres + .5f) / fullres;
	const float pixelCoC = DOF::SelectCoC(COCbuffer.Gather(COCdownsampleFilter, centerPoint));
#endif
#if 0
	const float layerSeparationCoC = max(abs(pixelCoC), DOF::minLayerSeparationCoC);
#else
	const float layerSeparationCoC = DOF::minLayerSeparationCoC;
#endif
	const float blendRange = layerSeparationCoC * DOF::layerBlendRangeScale;
	const float blendFar = /*0;*/ smoothstep(-blendRange, +blendRange, pos.z - layerSeparationCoC * sign(pos.z));

	Layers layers = { spriteColor, spriteColor };
	layers.near.a *= 1 - blendFar;
	layers.far.a *= blendFar;
	return layers;
}