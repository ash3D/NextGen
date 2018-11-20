#include "per-frame data.hlsli"

struct XformedVertex
{
	float3 viewDir		: ViewDir;
	float4 xformedPos	: SV_POSITION;
};

XformedVertex main(in float4 pos : POSITION)
{
	const float3 worldPos = mul(pos, terrainWorldXform);
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	const XformedVertex output = { -viewPos, mul(float4(viewPos, 1.f), projXform) };
	return output;
}