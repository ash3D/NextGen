// uniforms

float2 targetRes;

// textures

Texture2D tex;

// samplers

SamplerState texSampler
{
	Filter = MIN_MAG_LINEAR_MIP_POINT;
};

// structs

struct TVertex
{
	float4	color:		COLOR;
	float4	pos:		SV_POSITION;
};

struct TQuadDesc
{
	float4	posExtents:	POSITION_EXTENTS;
	float	order:		ORDER;
	float	angle:		ANGLE;
	float4	color:		QUAD_COLOR;
};

struct TEllipseQuadVertrex
{
	float4	color:		ELLIPSE_COLOR;
	float2	localPos:	LOCAL_POSITION;
	float4	pos:		SV_POSITION;
};

// helper functions

// [0..targetRes] -> [-1..+1]
inline float2 PixelSpace2NDCSpace(float2 coords)
{
	// move, scale => (coords - targetRes * .5) / (targetRes * .5)
	return 2 * coords / targetRes - 1;
}

void XformAxis(in float angle, in float2 scale, out float2 xAxis, out float2 yAxis)
{
	// rotate x axis
	sincos(angle, xAxis.y, xAxis.x);

	// get y axis from x axis
	yAxis = float2(-xAxis.y, +xAxis.x);

	// scale axis
	xAxis *= scale.x;
	yAxis *= scale.y;
}

inline bool InsideCircle(float2 pos)
{
	return dot(pos, pos) <= 1;
}

// vertex shaders

TVertex VS(in TVertex vertex)
{
	return vertex;
}

TQuadDesc QuadVS(in TQuadDesc quadDesc)
{
	return quadDesc;
}

VertexShader Quad_VS = CompileShader(vs_4_0, QuadVS());

// geometry shaders

[maxvertexcount(4)]
void QuadGS(in point TQuadDesc quadDesc[1], inout TriangleStream<TVertex> quad)
{
	// rotate and scale axis
	float2 x_axis, y_axis;
	XformAxis(quadDesc[0].angle, quadDesc[0].posExtents.zw, x_axis, y_axis);

	// expand quad

	TVertex vertex = {quadDesc[0].color, float4(PixelSpace2NDCSpace(quadDesc[0].posExtents.xy - x_axis - y_axis), quadDesc[0].order, 1)};
	quad.Append(vertex);

	vertex.pos.xy = PixelSpace2NDCSpace(quadDesc[0].posExtents.xy + x_axis - y_axis);
	quad.Append(vertex);

	vertex.pos.xy = PixelSpace2NDCSpace(quadDesc[0].posExtents.xy - x_axis + y_axis);
	quad.Append(vertex);

	vertex.pos.xy = PixelSpace2NDCSpace(quadDesc[0].posExtents.xy + x_axis + y_axis);
	quad.Append(vertex);
}

[maxvertexcount(4)]
void EllipseGS(in point TQuadDesc ellipseDesc[1], inout TriangleStream<TEllipseQuadVertrex> ellipseQuad)
{
	// rotate and scale axis
	float2 x_axis, y_axis;
	XformAxis(ellipseDesc[0].angle, ellipseDesc[0].posExtents.zw, x_axis, y_axis);

	// expand quad

	TEllipseQuadVertrex vertex = {ellipseDesc[0].color, float2(-1, -1), float4(PixelSpace2NDCSpace(ellipseDesc[0].posExtents.xy - x_axis - y_axis), ellipseDesc[0].order, 1)};
	ellipseQuad.Append(vertex);

	vertex.localPos = float2(+1, -1);
	vertex.pos.xy = PixelSpace2NDCSpace(ellipseDesc[0].posExtents.xy + x_axis - y_axis);
	ellipseQuad.Append(vertex);

	vertex.localPos = float2(-1, +1);
	vertex.pos.xy = PixelSpace2NDCSpace(ellipseDesc[0].posExtents.xy - x_axis + y_axis);
	ellipseQuad.Append(vertex);

	vertex.localPos = float2(+1, +1);
	vertex.pos.xy = PixelSpace2NDCSpace(ellipseDesc[0].posExtents.xy + x_axis + y_axis);
	ellipseQuad.Append(vertex);
}

GeometryShader Quad_GS = CompileShader(gs_4_0, QuadGS()), Ellipse_GS = CompileShader(gs_4_0, EllipseGS());

// pixel shaders

float4 PS(in float4 color: COLOR): SV_TARGET
{
	return color;
}

float4 TexturePS(in nointerpolation float4 color: COLOR, in noperspective float2 uv: TEXCOORD): SV_TARGET
{
	return color * tex.Sample(texSampler, uv);
}

float4 EllipsePS(in nointerpolation float4 color: ELLIPSE_COLOR, in noperspective float2 pos: LOCAL_POSITION): SV_TARGET
{
//	clip(1 - dot(pos, pos));
	if (!InsideCircle(pos)) discard;// slightly more efficient then clip
	return color;
}

float4 EllipseAAPS(in nointerpolation float4 color: ELLIPSE_COLOR, in noperspective float2 pos: LOCAL_POSITION, out uint mask: SV_COVERAGE): SV_TARGET
{
//	mask = ~0;
	const float2 corner_offset = mul((float2).5, abs(float2x2(ddx_coarse(pos), ddy_coarse(pos))));
	if (InsideCircle(abs(pos) + corner_offset))
		mask = ~0;
	else
	{
		mask = 0;
		if (InsideCircle(abs(pos) - corner_offset))
			for (uint sampleIdx = 0; sampleIdx < GetRenderTargetSampleCount(); sampleIdx++)
			{
				//const float2 samplePos = pos + mul(GetRenderTargetSamplePosition(sampleIdx), offset_xform);
				const float2 samplePos = EvaluateAttributeAtSample(pos, sampleIdx);
				if (InsideCircle(samplePos))
					mask |= 1 << sampleIdx;
			}
		else
			discard;// minor speedup
	}
	return color;
}

PixelShader Ellipse_PS = CompileShader(ps_4_0, EllipsePS()), EllipseAA_PS = CompileShader(ps_5_0, EllipseAAPS());

// rasterizer states

RasterizerState Rasterize2D
{
//	FillMode = Wireframe;
	Cullmode = None;
	MultisampleEnable = false;
	AntialiasedLineEnable = true;
};

technique10
{
	pass
	{
		SetVertexShader(CompileShader(vs_4_0, VS()));
		SetGeometryShader(NULL);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, PS()));
		SetRasterizerState(Rasterize2D);
	}
}

technique10 Rect
{
	pass
	{
		SetVertexShader(Quad_VS);
		SetGeometryShader(Quad_GS);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, PS()));
		SetRasterizerState(Rasterize2D);
	}
}

technique10 Ellipse
{
	pass
	{
		SetVertexShader(Quad_VS);
		SetGeometryShader(Ellipse_GS);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(Ellipse_PS);
		SetRasterizerState(Rasterize2D);
	}
}

technique10 EllipseAA
{
	pass
	{
		SetVertexShader(Quad_VS);
		SetGeometryShader(Ellipse_GS);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(EllipseAA_PS);
		SetRasterizerState(Rasterize2D);
	}
}