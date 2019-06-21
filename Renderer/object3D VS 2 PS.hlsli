#ifndef OBJECT3D_VS_2_PS_INCLUDED
#define OBJECT3D_VS_2_PS_INCLUDED

struct XformedVertex
{
	float3 viewDir : ViewDir, N : NORMAL;
};

struct XformedVertex_UV : XformedVertex
{
	float2 uv : TEXCOORD;
};

struct XformedVertex_UV_TG : XformedVertex_UV
{
	float3 tangents[2] : TANGENTS;
};

#endif	// OBJECT3D_VS_2_PS_INCLUDED