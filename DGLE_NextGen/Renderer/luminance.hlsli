/**
\author		Alexey Shaydurov aka ASH
\date		05.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#ifndef LUMINANCE_INCLUDED
#define LUMINANCE_INCLUDED

inline float RGB_2_luminance(float3 color)
{
    return dot(color, float3(.299f, .587f, .144f));
}

#endif  // LUMINANCE_INCLUDED