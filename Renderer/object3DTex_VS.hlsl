#include "object3D VS 2 PS.hlsli"
#include "object3DFlat_VS.hlsl"

struct SrcVertexTex : SrcVertex
{
	float2 UV	: TEXCOORD;
};

XformedVertexTex Tex_VS(in SrcVertexTex input, out float4 xformedPos : SV_POSITION, out float height : SV_ClipDistance)
{
	const XformedVertex baseVert = Flat_VS((SrcVertex)input, xformedPos, height);
	const XformedVertexTex output = { baseVert, input.UV };
	return output;
}