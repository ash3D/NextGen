#include "object3D VS 2 PS.hlsli"
#include "object3D_VS_UV.hlsl"

struct SrcVertex_UV_TG : SrcVertex_UV
{
	float3 tangents[2] : TANGENTS;
};

XformedVertex_UV_TG VS_UV_TB(in SrcVertex_UV_TG input, out float4 xformedPos : SV_POSITION, out float height : SV_ClipDistance)
{
	const XformedVertex_UV baseVert = VS_UV((SrcVertex_UV)input, xformedPos, height);
	
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
	const XformedVertex_UV_TG output = { baseVert, XformOrthoUniform(input.tangents[0]), XformOrthoUniform(input.tangents[1]) };
	return output;
}