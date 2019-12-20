#pragma once

namespace LensFlare
{
	static const float
		strength = 1e-2f,
		threshold = 1e-1f / strength,
		normRebalance = 1E4f/*to avoid fp16 blending underflow*/;

	struct Source
	{
		float2 pos : POSITION;
		float2 ext : EXTENTS;
		float2 rot : ROTATION;
		float3 edg : EDGE_CLIP_SETUP;
		float4 col : COLOR;
	};
}