/**
\author		Alexey Shaydurov aka ASH
\date		17.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#ifndef AABB_3D_INCLUDED
#define AABB_3D_INCLUDED

struct ClipSpaceAABB
{
	float4 corner : AABB_corner, extents[3] : AABB_extent;
};

#endif	// AABB_3D_INCLUDED