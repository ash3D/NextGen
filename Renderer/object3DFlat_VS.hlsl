#include "per-frame data.hlsli"
#include "object3D VS 2 PS.hlsli"

cbuffer InstanceData : register(b1)
{
	row_major float4x3 worldXform;
};

struct SrcVertex
{
	float4 pos	: POSITION;
	float3 N	: NORMAL;
};

// 2 view space
float3 XformOrthoUniform(float3 vec)
{
	return mul(mul(mul(vec, worldXform), terrainWorldXform), viewXform);
}

XformedVertex Flat_VS(in SrcVertex input, out float4 xformedPos : SV_POSITION, out float height : SV_ClipDistance)
{
	const float3 worldPos = mul(float4(mul(input.pos, worldXform), 1.f), terrainWorldXform);
	height = worldPos.z;	// do not render anything under terrain
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	xformedPos = mul(float4(viewPos, 1.f), projXform);

	/*
		xform normal
		!: use vector xform for now, need to replace with covector xform (inverse transpose) for correct non-uniform scaling handling
	*/
	const XformedVertex output = { -viewPos, XformOrthoUniform(input.N) };
	return output;
}