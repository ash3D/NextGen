#ifndef SAMPLERS_INCLUDED
#define SAMPLERS_INCLUDED

SamplerState terrainAlbedoSampler : register(s0), terrainFresnelSampler : register(s1), terrainRoughnessSampler : register(s2), terrainBumpSampler : register(s3);

#endif	// SAMPLERS_INCLUDED