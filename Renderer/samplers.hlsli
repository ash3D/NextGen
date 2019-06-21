#ifndef SAMPLERS_INCLUDED
#define SAMPLERS_INCLUDED

SamplerState
	terrainAlbedoSampler		: register(s0),
	terrainFresnelSampler		: register(s1),
	terrainRoughnessSampler		: register(s2),
	terrainBumpSampler			: register(s3),
	obj3DAlbedoSampler			: register(s4),
	obj3DAlbedoAlphatestSampler	: register(s5),
	obj3DBumpSampler			: register(s6),
	obj3DGlassMaskSampler		: register(s7),
	obj3DTVSampler				: register(s8);

#endif	// SAMPLERS_INCLUDED