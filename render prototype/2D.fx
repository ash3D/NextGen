/**
\author		Alexey Shaydurov aka ASH
\date		14.08.2016 (c)Alexey Shaydurov

This file is a part of DGLE2 project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE2.h" for more details.
*/

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

	TVertex vertex;
	vertex.color = quadDesc[0].color;
	vertex.pos.zw = float2(quadDesc[0].order, 1);

	vertex.pos.xy = PixelSpace2NDCSpace(quadDesc[0].posExtents.xy - x_axis - y_axis);
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

	TEllipseQuadVertrex vertex;
	vertex.color = ellipseDesc[0].color;
	vertex.pos.zw = float2(ellipseDesc[0].order, 1);

	vertex.localPos = float2(-1, -1);
	vertex.pos.xy = PixelSpace2NDCSpace(ellipseDesc[0].posExtents.xy - x_axis - y_axis);
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

// depthstencil states

DepthStencilState DepthLayer2D
{
	DepthFunc = Less_Equal;
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
		SetDepthStencilState(DepthLayer2D, ~0);
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
		SetDepthStencilState(DepthLayer2D, ~0);
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
		SetDepthStencilState(DepthLayer2D, ~0);
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
		SetDepthStencilState(DepthLayer2D, ~0);
	}
}

// test

ByteAddressBuffer buff;
RWByteAddressBuffer rwbuff, result;
RWTexture3D<uint4> rwtex;

tbuffer cb
{
	uint offset[16];
	uint4 data[128];
}

void f(uint i) {}
void f(uint i, out uint j) {}

[numthreads(512, 1, 1)]
void CS(uint3 id: SV_DispatchThreadID)
{
	uint adr = 0;id.x << 2;
	f(adr.xxx.zy);
	buff.Load4(uint(0) + float1x1(0));
	uint4 val = buff.Load4(0);
	[unroll]
	for (uint i = 0; i < 128; i++)
	{
		//rwbuff.Store4(adr + i*4*4/*offset[i]*/, val);
		//val += buff.Load(adr + i*4*4/*offset[i]*/);
		//val += rwtex[(uint3)(adr + i*4*4/*offset[i]*/)];
		//rwtex[(uint3)(adr + i*4*4/*offset[i]*/)] = val;
		val += data[i];
	}
	//result.Store4(0, val);
	float2x3 test=1;
	float3x2(test);
	test._m11_m12_m11_m11;
	float4x2 m={test, test._12_21};
	int1 x = m;
	float3 z = (float1x1)0 + (float4)0 + (float1x3)0;
	transpose((float4)0 + (float1x3)0);
	float2x2 M = (float4)0;
	M += z.xxxx;
	test[id.x];
	determinant(m);

	((int1)0 + (float1x1)0).x;
	((float1x1)0 + (int1)0).x;
	((int1x1)0 + (float1)0).x;
	((float1)0 + (int1x1)0).x;
	((float1)0 + (float1x1)0).x;
	((float1x1)0 + (float1)0)._11;

	((int1x3)0 + (float4)0).z;
	((float4)0 + (int1x3)0).z;
	((float1x3)0 + (float4)0)._13;
	((float4)0 + (float1x3)0)._13;
	((float3)0 + (float1x3)0).z;
	((float1x3)0 + (float3)0)._13;

	((float1x3)0 == (float4)0).z;

	((float4)0 += (int1x3)0).w;
	((float4)0 += (int1x3)0)._44;
}

technique11 test
{
	pass
	{
		SetComputeShader(CompileShader(cs_5_0, CS()));
	}
}