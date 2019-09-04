#include "object3D VS 2 PS.hlsli"
#include "object3D bump PS.hlsli"

float4 main(in XformedVertex_UV_TG input, in bool front : SV_IsFrontFace) : SV_TARGET
{
	return BumpPS(input, front, false);
}