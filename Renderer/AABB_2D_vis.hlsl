#include "camera params.hlsli"
#include "HDR codec.hlsli"

cbuffer Constants : register(b0, space1)
{
	float3 color;
}

ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1, space1);

float4 main() : SV_TARGET
{
	return EncodeLDRExp(color, cameraSettings.exposure);
}