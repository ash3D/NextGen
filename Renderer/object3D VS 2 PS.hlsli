/**
\author		Alexey Shaydurov aka ASH
\date		23.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#ifndef OBJECT3D_VS_2_PS_INCLUDED
#define OBJECT3D_VS_2_PS_INCLUDED

struct XformedVertex
{
	float3 viewDir : ViewDir, N : NORMAL;
};

#endif	// OBJECT3D_VS_2_PS_INCLUDED