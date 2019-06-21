#include "object3D VS 2 PS.hlsli"
#include "object3D tex PS.hlsli"

float4 main(in XformedVertex_UV input, in bool front : SV_IsFrontFace) : SV_TARGET
{
	return PS(input, front, false);
}