#pragma once

static const float
	lensFlareStrength = 1e-2f,
	lensFlareThreshold = 1e-1f / lensFlareStrength;

struct LensFlareSource
{
	float2 pos : POSITION;
	float2 ext : EXTENTS;
	float3 edg : EDGE_CLIP_SETUP;
	float4 col : COLOR;
};