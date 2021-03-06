#include "per-frame data.hlsli"

struct AABB
{
	float2 min : AABB_min, max : AABB_max;
};

/*
VS + hardware instancing approach currently used.
Quad expansion in GS allows to perform transform once per AABB rather than per quad vertex.
But there can be lack of AABBs to fill up VS/GS SIMD whereas instancing increases VS thread utilization by a factor of 4.
GPU should support packing of different instances into single SIMD (as AMD Vega does) for this approach to work efficiently, otherwise GS approach can result in better GPU utilization.
For larger batches GS approach can also lead to better results.
*/

float4 main(in uint quadVertexID : SV_VertexID, in AABB aabb) : SV_POSITION
{
	const float2 corner = lerp(aabb.min, aabb.max, uint2(quadVertexID & 1u, quadVertexID >> 1u));
	const float3 worldPos = mul(float4(corner, 0.f, 1.f), terrainWorldXform);
	const float3 viewPos = mul(float4(worldPos, 1.f), viewXform);
	return mul(float4(viewPos, 1.f), projXform);
}