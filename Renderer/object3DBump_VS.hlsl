#include "object3D VS 2 PS.hlsli"
#include "object3DTex_VS.hlsl"

struct SrcVertexBump : SrcVertexTex
{
	float3 tangents[2] : TANGENTS;
};

XformedVertexBump Bump_VS(in SrcVertexBump input, out float4 xformedPos : SV_POSITION, out float height : SV_ClipDistance)
{
	const XformedVertexTex baseVert = Tex_VS((SrcVertexTex)input, xformedPos, height);
	
	/*
		correct way to handle TBN transform in general case:
			- pass TBN untransformed to PS
			- normalize it in PS (after hw interpolation)
			- do normal mapping
			- transform resulting normal 2 view space (using covector xform that is inverse transpose)
			- normalize

		optimization for orthouniform transform (orthonormal + uniform scaling):
			- transform TBN 2 view space in VS
			- normalize it in PS (after hw interpolation)
			- do normal mapping
	*/
	const XformedVertexBump output = { baseVert, XformOrthoUniform(input.tangents[0]), XformOrthoUniform(input.tangents[1]), float2(length(input.tangents[0]), length(input.tangents[1])) };
	return output;
}