#include "per-frame data.hlsli"
#include "camera params.hlsli"
#include "lighting.hlsli"
#include "HDR codec.hlsli"

ConstantBuffer<CameraParams::Settings> cameraSettings : register(b0, space1);

cbuffer Constants : register(b1, space1)
{
	float3 albedo;
}

float4 main(in float3 viewDir : ViewDir) : SV_TARGET
{
	/*
		xform (0, 0, -1) normal by terrainWorldXform matrix, then by viewXform
		assumes both terrainWorldXform and viewXform are orthonormal, need inverse transpose otherwise (and normalize)
		current mul is suboptimal: viewXform[2] can be used if terrainWorldXform keeps Z line (which is the case) or merged in worldViewXform
	*/
	const float3 color = Lighting::Lit(albedo, .5f, Fresnel::F0(1.55f), mul(-terrainWorldXform[2], viewXform), normalize(viewDir), sun.dir, sun.irradiance);

	return EncodeHDRExp(color, cameraSettings.exposure);
}