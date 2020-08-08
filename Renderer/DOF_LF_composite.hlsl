#include "CS config.hlsli"
#include "DOF.hlsli"
#include "camera params.hlsli"
#include "HDR codec.hlsli"
#include "upsampleBlur.hlsli"

#define COMPOSITE_FORWARD_SATURATE 1
#define NORMALIZE_FAR 1
#define NORMALIZE_SMOOTH 1
#define NORMALIZE_FULLRES 0
#define SORT_FULLRES_LAYER 0
#define FULLRES_LAYER_MSAA_WEIGHTING 1
#define PREFETCH_LAYERS SORT_FULLRES_LAYER || NORMALIZE_FAR && NORMALIZE_SMOOTH

SamplerState tapFilter : register(s0);
SamplerState COCsampler : register(s1);
Texture2DMS<float> ZBuffer : register(t0);
Texture2D fullresScene : register(t1);
Texture2D halfresScene : register(t5);	// for halfres CoC buffer in alpha channel
Texture2D<float2> fullresCOCbuffer : register(t4);
Texture2DArray blurredLayers : register(t6);
Texture2D lensFlare : register(t7);
RWTexture2D<float4> dst : register(u0);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

static const float normBoostLimit = 4;

#if COMPOSITE_FORWARD_SATURATE
#if NORMALIZE_FAR
inline void CompositeNormalizedLayer(inout float4 target, in float4 layer)
{
	layer.a = min(layer.a, 1 - target.a);
	layer.rgb *= layer.a;
	target += layer;
}

void CompositeNormalizedLayer(inout float4 target, in float4 layer, in float blend)
{
	layer.a *= blend;
	CompositeNormalizedLayer(target, layer);
}

inline void NormalizeLayer(inout float4 layer)
{
	if (layer.a) layer.rgb /= layer.a;
}
#if NORMALIZE_SMOOTH
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
#else
#if NORMALIZE_FULLRES
void CompositeLayer(inout float4 target, inout float4 lastLayer, in float4 newLayer)
{
	if (newLayer.a)
	{
		if (lastLayer.a)
		{
			lastLayer.rgb /= lastLayer.a;
			lastLayer.a = min(lastLayer.a, 1 - target.a);
			lastLayer.rgb *= lastLayer.a;
			target += lastLayer;
		}
		lastLayer = newLayer;
	}
}
#else
void CompositeLayer(inout float4 target, inout float3 lastLayer, in float4 newLayer)
{
	if (newLayer.a)
	{
		newLayer.rgb /= newLayer.a;
		lastLayer = newLayer;
		CompositeNormalizedLayer(target, newLayer);
	}
}
#endif
#endif
#else
void CompositeLayer(inout float4 target, in float4 layer)
{
	if (layer.a)
	{
		layer.rgb /= layer.a;
		layer.a = min(layer.a, 1 - target.a);
		layer.rgb *= layer.a;
		target += layer;
	}
}
#endif
#else
inline void Normalize(inout float4 target)
{
	if (target.a) target /= target.a;
}

inline void CompositeLayer(inout float4 target, in float4 layer)
{
	target *= saturate(1 - layer.a);
	target += layer;
}

#if SORT_FULLRES_LAYER
inline void CompositeLayer(inout float4 target, in float4 layer, in float blend)
{
	CompositeLayer(target, layer * blend);
}
#endif
#endif

#if !PREFETCH_LAYERS
float4 UpsampleLayer(float2 uv, DOF::LayerID layerID)
{
	return UpsampleBlur4(blurredLayers, tapFilter, float3(uv, layerID));
}
#endif

#if FULLRES_LAYER_MSAA_WEIGHTING
inline float SampleCOC(uint sampleIdx, uint2 coord)
{
	return cameraSettings.COC(ZBuffer.sample[sampleIdx][coord]) * 2/*to fullres pixels*/;
}

inline float FullresOpacity(float fullresCoC, float upsampleCoC, uint2 coord, float2 center)
{
	uint2 resolution;
	uint MSAA;
	ZBuffer.GetDimensions(resolution.x, resolution.y, MSAA);

#if FULLRES_LAYER_MSAA_WEIGHTING == 1
	float opacity_MSAA = 0;

	for (uint sampleIdx = 0; sampleIdx < MSAA; sampleIdx++)
		opacity_MSAA += saturate(DOF::Opacity(SampleCOC(sampleIdx, coord), cameraSettings.aperture));

	opacity_MSAA /= MSAA;
#elif FULLRES_LAYER_MSAA_WEIGHTING == 2
	float CoC_MSAA = 0;

	for (uint sampleIdx = 0; sampleIdx < MSAA; sampleIdx++)
		CoC_MSAA = max(CoC_MSAA, abs(SampleCOC(sampleIdx, coord)));

	const float opacity_MSAA = saturate(DOF::Opacity(CoC_MSAA, cameraSettings.aperture));
#elif FULLRES_LAYER_MSAA_WEIGHTING == 3
	float transp = 0;

	for (uint sampleIdx = 0; sampleIdx < MSAA; sampleIdx++)
		transp += rcp(saturate(DOF::Opacity(SampleCOC(sampleIdx, coord), cameraSettings.aperture)));

	const float opacity_MSAA = MSAA / transp;
#elif FULLRES_LAYER_MSAA_WEIGHTING == 4
	float transp = 0;

	for (uint sampleIdx = 0; sampleIdx < MSAA; sampleIdx++)
		transp += rcp(DOF::Opacity(SampleCOC(sampleIdx, coord), cameraSettings.aperture));

	const float opacity_MSAA = saturate(MSAA / transp);
#elif FULLRES_LAYER_MSAA_WEIGHTING == 5
	float CoC_MSAA = 0;

	for (uint sampleIdx = 0; sampleIdx < MSAA; sampleIdx++)
		CoC_MSAA += abs(SampleCOC(sampleIdx, coord));

	CoC_MSAA /= MSAA;
	const float opacity_MSAA = saturate(DOF::Opacity(CoC_MSAA, cameraSettings.aperture));
#else
#error wrong FULLRES_LAYER_MSAA_WEIGHTING
#endif

#if 0
	return opacity_MSAA;
#elif 0
	return min(saturate(DOF::Opacity(fullresCoC, cameraSettings.aperture)), opacity_MSAA);
#elif 0
	return min(saturate(DOF::Opacity(upsampleCoC, cameraSettings.aperture)), opacity_MSAA);
#elif 0
	return min(saturate(DOF::Opacity(halfresScene[coord / 2].a * 2/*to fullres pixels*/, cameraSettings.aperture)), opacity_MSAA);
#else
	return min(saturate(DOF::Opacity(halfresScene.SampleLevel(COCsampler, center, 0).a * 2/*to fullres pixels*/, cameraSettings.aperture)), opacity_MSAA);
#endif
}
#else
inline float FullresOpacity(float CoC, float, uint2, float2)
{
	return saturate(DOF::Opacity(CoC, cameraSettings.aperture));
}
#endif

[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	float2 dstSize;
	dst.GetDimensions(dstSize.x, dstSize.y);
	const float2 center = (coord + .5f) / dstSize;

	const float upsampleCoC = UpsampleBlurA(halfresScene, tapFilter, center);
#if 0
	const float layerSeparationCoC = max(abs(upsampleCoC), DOF::minLayerSeparationCoC);
#else
	const float layerSeparationCoC = DOF::minLayerSeparationCoC;
#endif
#define FULLRES_COC_AVG 1
	const float fullresCoC = fullresCOCbuffer[coord][FULLRES_COC_AVG]; // in halfres pixels

#if PREFETCH_LAYERS
#if !COMPOSITE_FORWARD_SATURATE
	const
#endif
	float4 upsampleLayers[4] =
	{
		UpsampleBlur4(blurredLayers, tapFilter, float3(center, 0)),
		UpsampleBlur4(blurredLayers, tapFilter, float3(center, 1)),
		UpsampleBlur4(blurredLayers, tapFilter, float3(center, 2)),
		UpsampleBlur4(blurredLayers, tapFilter, float3(center, 3))
	};
#endif

#if COMPOSITE_FORWARD_SATURATE
#if NORMALIZE_FAR
#if NORMALIZE_SMOOTH
#if NORMALIZE_FULLRES
#error unimplemented
#else
	float4 composition = 0;

#if SORT_FULLRES_LAYER
	NormalizeLayer(upsampleLayers[0]);
	NormalizeLayer(upsampleLayers[1]);
	NormalizeLayer(upsampleLayers[2]);
	NormalizeLayer(upsampleLayers[3]);

	// nearest layer, always treat as foreground
	CompositeNormalizedLayer(composition, upsampleLayers[DOF::FOREGROUND_NEAR_LAYER]);

	float2 blendFactors = smoothstep(float2(-layerSeparationCoC, 0), float2(0, +layerSeparationCoC), fullresCoC);
	CompositeNormalizedLayer(composition, upsampleLayers[DOF::FOREGROUND_FAR_LAYER], blendFactors.x);
	CompositeNormalizedLayer(composition, upsampleLayers[DOF::BACKGROUND_NEAR_LAYER], blendFactors.y);

	float4 fullresLayer = float4(DecodeHDRExp(fullresScene[coord], cameraSettings.exposure), FullresOpacity(fullresCoC * 2/*to fullres pixels*/, upsampleCoC * 2/*to fullres pixels*/, coord, center));
	CompositeNormalizedLayer(composition, fullresLayer);

	blendFactors = 1 - blendFactors;
	CompositeNormalizedLayer(composition, upsampleLayers[DOF::FOREGROUND_FAR_LAYER], blendFactors.x);
	CompositeNormalizedLayer(composition, upsampleLayers[DOF::BACKGROUND_NEAR_LAYER], blendFactors.y);

	// farthest layer, always treat as background
	CompositeNormalizedLayer(composition, upsampleLayers[DOF::BACKGROUND_FAR_LAYER]);
#else
	CompositeLayer(composition, upsampleLayers[DOF::FOREGROUND_NEAR_LAYER]);
	CompositeLayer(composition, upsampleLayers[DOF::FOREGROUND_FAR_LAYER]);

	float4 fullresLayer = float4(DecodeHDRExp(fullresScene[coord], cameraSettings.exposure), FullresOpacity(fullresCoC * 2/*to fullres pixels*/, upsampleCoC * 2/*to fullres pixels*/, coord, center));
	CompositeNormalizedLayer(composition, fullresLayer);

	CompositeLayer(composition, upsampleLayers[DOF::BACKGROUND_NEAR_LAYER]);
	CompositeLayer(composition, upsampleLayers[DOF::BACKGROUND_FAR_LAYER]);
#endif

	CompensateOcclusion(composition, upsampleLayers[DOF::BACKGROUND_FAR_LAYER]);
	CompensateOcclusion(composition, upsampleLayers[DOF::BACKGROUND_NEAR_LAYER]);
	CompensateOcclusion(composition, upsampleLayers[DOF::FOREGROUND_FAR_LAYER]);
	CompensateOcclusion(composition, upsampleLayers[DOF::FOREGROUND_NEAR_LAYER]);

#if 1
	composition.rgb /= composition.a;
#endif
#endif
#else
#if SORT_FULLRES_LAYER
#error unimplemented
#else
#if NORMALIZE_FULLRES
	float4 composition = 0, lastlayer = UpsampleLayer(center, DOF::FOREGROUND_NEAR_LAYER);

	CompositeLayer(composition, lastlayer, UpsampleLayer(center, DOF::FOREGROUND_FAR_LAYER));

	float4 fullresLayer = float4(DecodeHDRExp(fullresScene[coord], cameraSettings.exposure), FullresOpacity(fullresCoC * 2/*to fullres pixels*/, upsampleCoC * 2/*to fullres pixels*/, coord, center));
	//fullresLayer.rgb = float3(1, 0, 0);
	//fullresLayer.rgb = UpsampleBlur3(halfresScene, tapFilter, center);
	fullresLayer.rgb *= fullresLayer.a;
	CompositeLayer(composition, lastlayer, fullresLayer);

	CompositeLayer(composition, lastlayer, UpsampleLayer(center, DOF::BACKGROUND_NEAR_LAYER));
	CompositeLayer(composition, lastlayer, UpsampleLayer(center, DOF::BACKGROUND_FAR_LAYER));

	lastlayer.rgb *= (1 - composition.a) / lastlayer.a;
	composition.rgb += lastlayer.rgb;
#else
	float4 composition = 0;
	float3 lastlayer = 0;

	CompositeLayer(composition, lastlayer, UpsampleLayer(center, DOF::FOREGROUND_NEAR_LAYER));
	CompositeLayer(composition, lastlayer, UpsampleLayer(center, DOF::FOREGROUND_FAR_LAYER));

	float4 fullresLayer = float4(DecodeHDRExp(fullresScene[coord], cameraSettings.exposure), FullresOpacity(fullresCoC * 2/*to fullres pixels*/, upsampleCoC * 2/*to fullres pixels*/, coord, center));
	CompositeNormalizedLayer(composition, fullresLayer);

	CompositeLayer(composition, lastlayer, UpsampleLayer(center, DOF::BACKGROUND_NEAR_LAYER));
	CompositeLayer(composition, lastlayer, UpsampleLayer(center, DOF::BACKGROUND_FAR_LAYER));

	lastlayer *= 1 - composition.a;
	composition.rgb += lastlayer;
#endif
#endif
#endif
#else
#if SORT_FULLRES_LAYER
#error unimplemented
#else
	float4 composition = 0;

	CompositeLayer(composition, UpsampleLayer(center, DOF::FOREGROUND_NEAR_LAYER));
	CompositeLayer(composition, UpsampleLayer(center, DOF::FOREGROUND_FAR_LAYER));

	float4 fullresLayer = float4(DecodeHDRExp(fullresScene[coord], cameraSettings.exposure), FullresOpacity(fullresCoC * 2/*to fullres pixels*/, upsampleCoC * 2/*to fullres pixels*/, coord, center));
	fullresLayer.rgb *= fullresLayer.a;
	CompositeLayer(composition, fullresLayer);

	CompositeLayer(composition, UpsampleLayer(center, DOF::BACKGROUND_NEAR_LAYER));
	CompositeLayer(composition, UpsampleLayer(center, DOF::BACKGROUND_FAR_LAYER));

	composition.rgb /= composition.a;
#endif
#endif
#else
#if SORT_FULLRES_LAYER
	/*
	!
	layers blending factors derived from filtered upsampled CoC can cause artifacts
	better approach seemed is to blend per point sample then filter
	or abandon per-pixel layer separation and instead rely on global separation
	it will also simplify splatting PS and allow for sharing root signature with lens flare
	*/

	// farthest layer, always treat as background
	float4 composition = upsampleLayers[DOF::BACKGROUND_FAR_LAYER];
	Normalize(composition);

	float2 blendFactors = smoothstep(float2(+layerSeparationCoC, 0), float2(0, -layerSeparationCoC), fullresCoC);
#if 0
	blendFactors = round(blendFactors);
#endif
	CompositeLayer(composition, upsampleLayers[DOF::BACKGROUND_NEAR_LAYER], blendFactors.x);
	Normalize(composition);
	CompositeLayer(composition, upsampleLayers[DOF::FOREGROUND_FAR_LAYER], blendFactors.y);
	Normalize(composition);

	float4 fullresLayer = float4(DecodeHDRExp(fullresScene[coord], cameraSettings.exposure), FullresOpacity(fullresCoC * 2/*to fullres pixels*/, upsampleCoC * 2/*to fullres pixels*/, coord, center));
	fullresLayer.rgb *= fullresLayer.a;
	CompositeLayer(composition, fullresLayer);
	Normalize(composition);

	blendFactors = 1 - blendFactors;
	CompositeLayer(composition, upsampleLayers[DOF::BACKGROUND_NEAR_LAYER], blendFactors.x);
	Normalize(composition);
	CompositeLayer(composition, upsampleLayers[DOF::FOREGROUND_FAR_LAYER], blendFactors.y);
	Normalize(composition);

	// nearest layer, always treat as foreground
	CompositeLayer(composition, upsampleLayers[DOF::FOREGROUND_NEAR_LAYER]);
	Normalize(composition);
#else
	float4 composition = UpsampleLayer(center, DOF::BACKGROUND_FAR_LAYER);
	Normalize(composition);

	CompositeLayer(composition, UpsampleLayer(center, DOF::BACKGROUND_NEAR_LAYER));
	Normalize(composition);

	float4 fullresLayer = float4(DecodeHDRExp(fullresScene[coord], cameraSettings.exposure), FullresOpacity(fullresCoC * 2/*to fullres pixels*/, upsampleCoC * 2/*to fullres pixels*/, coord, center));
	fullresLayer.rgb *= fullresLayer.a;
	CompositeLayer(composition, fullresLayer);
	Normalize(composition);

	CompositeLayer(composition, UpsampleLayer(center, DOF::FOREGROUND_FAR_LAYER));
	Normalize(composition);

	CompositeLayer(composition, UpsampleLayer(center, DOF::FOREGROUND_NEAR_LAYER));
	Normalize(composition);
#endif
#endif

	// composite lens flare
	composition.rgb += lensFlare.SampleLevel(tapFilter, center, 0);

	//composition = fullresLayer;
	//composition = upsampleLayers[DOF::FOREGROUND_NEAR_LAYER];
	//Normalize(composition);
	// encode HDR to enable hw bilinear Karis fetches for subsequent bloom downsample
	dst[coord] = EncodeHDR(composition);
	//dst[coord] = upsampleCoC;
	//dst[coord] = fullresCoC;
	//dst[coord] = fullresLayer.a;
	//dst[coord] = blendFactors.rgrg;
	//dst[coord] = upsampleLayers[DOF::BACKGROUND_NEAR_LAYER].a;
	//dst[coord] = composition.a;
	//dst[coord] = fullresLayer.a*1000;
	//dst[coord] = upsampleLayers[0].a + upsampleLayers[1].a + upsampleLayers[2].a + upsampleLayers[3].a;
	//dst[coord] = saturate(DOF::Opacity(halfresScene[coord / 2].a * 2/*to fullres pixels*/, cameraSettings.aperture));
	//dst[coord] = FullresOpacity(fullresCoC * 2/*to fullres pixels*/, upsampleCoC * 2/*to fullres pixels*/, coord, center);
}