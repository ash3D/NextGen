namespace Materials
{
	enum TextureID
	{
		TV_SCREEN,
	};
}

#include "object3D VS 2 PS.hlsli"
#include "object3D tex stuff.hlsli"
#include "object3D material params.hlsli"
#include "object3DTex_VS.hlsl"

ConstantBuffer<Materials::TV> materialParams : register(b0, space1);

XformedVertexTex TV_VS(in SrcVertexTex input, out float4 xformedPos : SV_POSITION, out float height : SV_ClipDistance)
{
	float2 signalSize;
	SelectTexture(Materials::TV_SCREEN).GetDimensions(signalSize.x, signalSize.y);
	const float aspectsMismatch = materialParams.aspectRatio / signalSize.x * signalSize.y;
	const float2 rescale = max(float2(aspectsMismatch, rcp(aspectsMismatch)), 1);

	XformedVertexTex output = Tex_VS((SrcVertexTex)input, xformedPos, height);
	output.uv *= rescale;
	output.uv += .5f - rescale * .5f;
	return output;
}