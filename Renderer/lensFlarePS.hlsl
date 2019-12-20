half4 main(nointerpolation half4 spriteColor : COLOR, noperspective float2 circleDir : CLIP_CIRCLE_DIR) : SV_TARGET
{
	const float R2 = dot(circleDir, circleDir);
	clip(1 - R2);
	return spriteColor;
}