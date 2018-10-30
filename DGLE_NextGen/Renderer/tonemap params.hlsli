/**
\author		Alexey Shaydurov aka ASH
\date		30.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#ifndef TONEMAP_PARAMS_INCLUDED
#define TONEMAP_PARAMS_INCLUDED

struct TonemapParams
{
    float exposure, whitePoint, whitePointFactor /*1/whitePoint^2*/;
};

#endif	// TONEMAP_PARAMS_INCLUDED