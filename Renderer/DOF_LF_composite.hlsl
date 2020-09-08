#include "CS config.hlsli"
#include "DOF.hlsli"
#include "camera params.hlsli"
#include "HDR codec.hlsli"
#include "upsampleBlur.hlsli"

SamplerState tapFilter : register(s0);
SamplerState COCsampler : register(s1);
Texture2DMS<float> ZBuffer : register(t0);
Texture2D fullresScene : register(t1);
Texture2D halfresScene : register(t5);	// for halfres CoC buffer in alpha channel
Texture2DArray blurredLayers : register(t6);
Texture2D lensFlare : register(t7);
RWTexture2D<float4> dst : register(u0);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

static const float normBoostLimit = 4;

inline void CompositeNormalizedLayer(inout float4 target, in float4 layer)
{
	layer.a = min(layer.a, 1 - target.a);
	layer.rgb *= layer.a;
	target += layer;
}

void CompositeLayer(inout float4 target, inout float4 layer/*gets normalized*/)
{
	if (layer.a)
	{
		layer.rgb /= layer.a;
		CompositeNormalizedLayer(target, layer);
	}
}

void CompensateOcclusion(inout float4 target, in float4 layer/*normalized*/)
{
	layer.a *= min(saturate(1 - target.a) / layer.a, normBoostLimit);
	layer.rgb *= layer.a;
	target += layer;
}

inline float SampleCOC(uint sampleIdx, uint2 coord)
{
	return cameraSettings.COC(ZBuffer.sample[sampleIdx][coord]) * 2/*to fullres pixels*/;
}

inline float FullresOpacity(uint2 coord, float2 center)
{
	uint2 resolution;
	uint MSAA;
	ZBuffer.GetDimensions(resolution.x, resolution.y, MSAA);

	float opacity_MSAA = 0;

	for (uint sampleIdx = 0; sampleIdx < MSAA; sampleIdx++)
		opacity_MSAA += saturate(DOF::Opacity(SampleCOC(sampleIdx, coord), cameraSettings.aperture));

	opacity_MSAA /= MSAA;

	return min(saturate(DOF::Opacity(halfresScene.SampleLevel(COCsampler, center, 0).a * 2/*to fullres pixels*/, cameraSettings.aperture)), opacity_MSAA);
}

[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	float2 dstSize;
	dst.GetDimensions(dstSize.x, dstSize.y);
	const float2 center = (coord + .5f) / dstSize;

	// gets normalized during forward pass
	float4 upsampleLayers[4] =
	{
		UpsampleBlur4(blurredLayers, tapFilter, float3(center, 0)),
		UpsampleBlur4(blurredLayers, tapFilter, float3(center, 1)),
		UpsampleBlur4(blurredLayers, tapFilter, float3(center, 2)),
		UpsampleBlur4(blurredLayers, tapFilter, float3(center, 3))
	};

	float4 composition = 0;

	CompositeLayer(composition, upsampleLayers[DOF::FOREGROUND_NEAR_LAYER]);
	CompositeLayer(composition, upsampleLayers[DOF::FOREGROUND_FAR_LAYER]);

	const float4 fullresLayer = float4(DecodeHDRExp(fullresScene[coord], cameraSettings.exposure), FullresOpacity(coord, center));
	CompositeNormalizedLayer(composition, fullresLayer);

	CompositeLayer(composition, upsampleLayers[DOF::BACKGROUND_NEAR_LAYER]);
	CompositeLayer(composition, upsampleLayers[DOF::BACKGROUND_FAR_LAYER]);

	CompensateOcclusion(composition, upsampleLayers[DOF::BACKGROUND_FAR_LAYER]);
	CompensateOcclusion(composition, upsampleLayers[DOF::BACKGROUND_NEAR_LAYER]);
	CompensateOcclusion(composition, upsampleLayers[DOF::FOREGROUND_FAR_LAYER]);
	CompensateOcclusion(composition, upsampleLayers[DOF::FOREGROUND_NEAR_LAYER]);

#if 1
	composition.rgb /= composition.a;
#endif

	// composite lens flare
	composition.rgb += lensFlare.SampleLevel(tapFilter, center, 0);

	// encode HDR to enable hw bilinear Karis fetches for subsequent bloom downsample
	dst[coord] = EncodeHDR(composition);
}