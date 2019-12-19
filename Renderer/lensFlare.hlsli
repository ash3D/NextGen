#pragma once

static const float
	lensFlareStrength = 4e-8f,
	lensFlareThreshold = 4e-7f / lensFlareStrength;

struct LensFlareSource
{
	float2 pos : POSITION;
	float2 ext : EXTENTS;
	float3 edg : EDGE_CLIP_SETUP;
	float4 col : COLOR;
};