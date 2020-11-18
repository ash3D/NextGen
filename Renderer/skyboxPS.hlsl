#include "per-frame data.hlsli"
#include "camera params.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<CameraParams::Settings> cameraSettings : register(b0, space1);

SamplerState cubemapSampler : register(s0);
TextureCube cubemap : register(t0);

float4 main(in float3 dir : CUBEMAP_SPACE_DIR) : SV_TARGET
{
	return EncodeHDRExp(cubemap.Sample(cubemapSampler, dir) * skyLuminanceScale, cameraSettings.exposure);
}