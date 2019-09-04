#pragma once

namespace TerrainSamplers
{
	SamplerState
		albedo		: register(s0),
		fresnel		: register(s1),
		roughness	: register(s2),
		bump		: register(s3);
}