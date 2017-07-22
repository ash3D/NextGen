/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

float4 main(in float4 pos : SV_POSITION, in float2 uv : TEXCOORD) : SV_TARGET
{
	int2 i;
	return float4(0.f, modf(abs(uv * 1.e-1f), i), 1.f);
}