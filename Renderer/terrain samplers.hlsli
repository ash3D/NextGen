#ifndef TERRAIN_SAMPLERS_INCLUDED
#define TERRAIN_SAMPLERS_INCLUDED

namespace TerrainSamplers
{
	SamplerState
		albedo		: register(s0),
		fresnel		: register(s1),
		roughness	: register(s2),
		bump		: register(s3);
}

#endif	// TERRAIN_SAMPLERS_INCLUDED