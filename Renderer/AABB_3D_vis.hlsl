#include "tonemap params.hlsli"
#include "HDR codec.hlsli"

cbuffer Colors : register(b0)
{
	float3 colors[3]/*culled - visible on cull phase 1 - visible on cull phase 2*/;
}

cbuffer Offset : register(b1)
{
	uint visibilityOffset;	// in bytes
}

ConstantBuffer<TonemapParams> tonemapParams : register(b2);

#if 0
ByteAddressBuffer visibility[2] : register(t0);
#else
// consider placing it to descriptor table and using array of 2 buffers
ByteAddressBuffer visibilityPhase1 : register(t0), visibilityPhase2 : register(t1);
#endif

float4 main() : SV_TARGET
{
	const uint colorIdx = visibilityPhase1.Load(visibilityOffset) | visibilityPhase2.Load(visibilityOffset) << 1u;
	return EncodeLDR(colors[colorIdx], tonemapParams.exposure);
}