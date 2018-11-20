#ifndef AABB_3D_INCLUDED
#define AABB_3D_INCLUDED

struct ClipSpaceAABB
{
	float4 corner : AABB_corner, extents[3] : AABB_extent;
};

#endif	// AABB_3D_INCLUDED