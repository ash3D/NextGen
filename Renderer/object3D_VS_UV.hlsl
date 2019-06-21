#include "object3D VS 2 PS.hlsli"
#include "object3D_VS.hlsl"

struct SrcVertex_UV : SrcVertex
{
	float2 UV	: TEXCOORD;
};

XformedVertex_UV VS_UV(in SrcVertex_UV input, out float4 xformedPos : SV_POSITION, out float height : SV_ClipDistance)
{
	const XformedVertex baseVert = VS((SrcVertex)input, xformedPos, height);
	const XformedVertex_UV output = { baseVert, input.UV };
	return output;
}