#ifndef PER_FRAME_DATA_INCLUDED
#define PER_FRAME_DATA_INCLUDED

cbuffer PerFrameData : register(b0)
{
	row_major float4x4 projXform : packoffset(c0);
	row_major float4x3 viewXform : packoffset(c4), terrainWorldXform : packoffset(c8);
	float skyLuminanceScale : packoffset(c12);
	struct
	{
		float3 dir, irradiance;
	} sun : packoffset(c13);
	float3 projParams/*F, 1 / zn, 1 / zn - 1 / zf*/ : packoffset(c15);
	float3 camAdaptationFactors/*LumBright, LumDark, Focus*/ : packoffset(c16);
};

#endif	// PER_FRAME_DATA_INCLUDED