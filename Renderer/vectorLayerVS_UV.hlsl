#include "per-frame data.hlsli"

struct XformedVertex
{
	float3 viewDir		: ViewDir;
	float2 uv			: UV;
	float4 xformedPos	: SV_POSITION;
};

cbuffer Constants : register(b0, space2)
{
	float texScale;
}

XformedVertex main(in float4 pos : POSITION)
{
	const float3 worldPos = mul(pos, terrainWorldXform);
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	const XformedVertex output = { -viewPos, pos.xy * texScale, mul(float4(viewPos, 1.f), projXform) };
	return output;
}