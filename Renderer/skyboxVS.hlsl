#include "per-frame data.hlsli"

void main(in uint boxVertexID : SV_VertexID, out float3 cubeDir : CUBEMAP_SPACE_DIR, out float4 projPos : SV_POSITION)
{
	// [0..1] -> [-1..1]
	const float3 worldDir = float3(boxVertexID & 1u, boxVertexID >> 1u & 1u, boxVertexID >> 2u & 1u) * 2 - 1;
	
	// swap Y <--> Z (convert world space -> cubemap)
	cubeDir = worldDir.xzy;

	// dir.w == 0 so drop last xforms rows
	const float3 viewDir = mul(worldDir, (float3x3)viewXform);
	projPos = mul(viewDir, (float3x4)projXform);
}