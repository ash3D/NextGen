#pragma once

struct XformedVertex
{
	float3 viewDir : ViewDir, N : NORMAL;
};

struct XformedVertexTex : XformedVertex
{
	float2 uv : TEXCOORD;
};

struct XformedVertexBump : XformedVertexTex
{
	float3 tangents[2]	: TANGENTS;
	float2 normalScale	: BumpStrength;
};