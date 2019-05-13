#include "per-frame data.hlsli"

struct XformedVertex
{
	float3 viewDir		: ViewDir;
	float2 uv			: UV;
	float4 xformedPos	: SV_POSITION;
};

cbuffer QuadParams : register(b0, space2)
{
	float2 quadTexgenReduction;	// zero fractional part so doesn't affect texcoord wrapping
}

/*
	lower frequency of update (layer vs quad) so place in dedicated root signature slot
	AMD GCN can benefit from placing it into CB (which can be combined with IV/VB) rather than root constants
		although space would be wasted considering CB requires 256-byte alignment
		so this approach will became more resonable if more layer data required
*/
cbuffer LayerParams : register(b1, space2)
{
	float texScale;
}

XformedVertex main(in float4 pos : POSITION)
{
	const float3 worldPos = mul(pos, terrainWorldXform);
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	const XformedVertex output = { -viewPos, pos.xy * texScale - quadTexgenReduction, mul(float4(viewPos, 1.f), projXform) };
	return output;
}