struct Layers
{
	half4 near : SV_Target0, far : SV_Target1;
};

Layers main(nointerpolation half4 spriteColor : COLOR, noperspective float2 circleDir : CLIP_CIRCLE_DIR, noperspective float4 pos/*.z is blend near*/ : SV_Position,
	nointerpolation float R : CLIP_CIRCLE_R,
	noperspective float apertureMainEdgeFade : EDGE_MAIN_FADE,
	noperspective float apertureSideEdgesFade[2] : EDGE_SIDE_FADE
)
{
	const float circleDist = R - length(circleDir);
	clip(circleDist);

	float edgeFade = 1;
	edgeFade *= saturate(apertureMainEdgeFade);
	edgeFade *= saturate(apertureSideEdgesFade[0]);
	edgeFade *= saturate(apertureSideEdgesFade[1]);

	const float circleFade = saturate(circleDist);
	const float fade = min(edgeFade, circleFade);

	Layers layers = { spriteColor, spriteColor };
	layers.near *= fade * pos.z;
	layers.far *= fade * (1 - pos.z);
	return layers;
}