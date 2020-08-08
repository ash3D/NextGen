#ifndef PER_FRAME_DATA_INCLUDED
#define PER_FRAME_DATA_INCLUDED

cbuffer PerFrameData : register(b0)
{
	row_major float4x4 projXform;
	row_major float4x3 viewXform, terrainWorldXform;
	struct
	{
		float3 dir, irradiance;
	} sun;
	float3 projParams/*F, 1 / zn, 1 / zn - 1 / zf*/;
};

#endif	// PER_FRAME_DATA_INCLUDED