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
};

#endif	// PER_FRAME_DATA_INCLUDED