#pragma once

inline float RGB_2_luminance(float3 color)
{
	return dot(color, float3(.299f, .587f, .144f));
}