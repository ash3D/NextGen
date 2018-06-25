/**
\author		Alexey Shaydurov aka ASH
\date		25.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

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