/**
\author		Alexey Shaydurov aka ASH
\date		21.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

cbuffer Colors : register(b0)
{
	float3 colors[3]/*culled - visible on cull phase 1 - visible on cull phase 2*/;
}

cbuffer Offset : register(b1)
{
	uint visibilityOffset;	// in bytes
}

#if 0
ByteAddressBuffer visibility[2] : register(t0);
#else
// consider placing it to descriptor table and using array of 2 buffers
ByteAddressBuffer visibilityPhase1 : register(t0), visibilityPhase2 : register(t1);
#endif

float4 main() : SV_TARGET
{
	const uint colorIdx = visibilityPhase1.Load(visibilityOffset) | visibilityPhase2.Load(visibilityOffset) << 1u;
	return float4(colors[colorIdx], 1.f);
}