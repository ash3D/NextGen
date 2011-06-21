struct TVertex
{
	float4 color:	COLOR;
	float4 coords:	SV_POSITION;
};

TVertex VS(in TVertex vertex)
{
	return vertex;
}

float4 PS(float4 color: COLOR): SV_TARGET
{
	return color;
}

technique10
{
	pass
	{
		SetVertexShader(CompileShader(vs_4_0, VS()));
		SetGeometryShader(NULL);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, PS()));
	}
}