#include "lensFlare.hlsli"
#include "DOF.hlsli"
#include "Bokeh.hlsli"
#include "camera params.hlsli"
#include "HDR codec.hlsli"

SamplerState tapFilter : register(s0);
SamplerState bilateralTapSampler : register(s1);
Texture2D src : register(t1);
Texture2D<float2> COCbuffer : register(t4);
RWTexture2D<float4> dst : register(u5);
RWTexture2DArray<float4> blurredLayers : register(u6);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

static const int holeFillingBlurBand = 4;
static const float antileakFactor = 10;	// rcp(max leak percentage)

inline float Vignette(float2 ndc)
{
	const float fade = saturate(1.8f - dot(ndc, ndc));
	return fade * fade;
}

inline float UnpackGatherBlock(in float4 block, uniform uint2 idx)
{
	static const uint remapLUT[2][2] =
	{
		3, 2,
		0, 1
	};
	return block[remapLUT[idx.x][idx.y]];
}

void FetchBlock(inout float4 corner, inout float4 center, uniform int2 offset/*right bottom*/, uniform uint2 centerIdx, in float2 cornerPoint, in float4 bilateralWeights)
{
	float4 block[2][2] =
	{
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(1, 1)),
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(0, 1)),
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(1, 0)),
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(0, 0))
	};
	block[0][0] *= UnpackGatherBlock(bilateralWeights, uint2(0, 0));
	block[0][1] *= UnpackGatherBlock(bilateralWeights, uint2(0, 1));
	block[1][0] *= UnpackGatherBlock(bilateralWeights, uint2(1, 0));
	block[1][1] *= UnpackGatherBlock(bilateralWeights, uint2(1, 1));
	center += block[centerIdx.x][centerIdx.y];
	corner += block[0][0] + block[0][1] + block[1][0] + block[1][1];
}

float4 BilateralWeights(float4 CoCs, float targetCoC, float dilatedCoC)
{
	float4 weights = dilatedCoC / CoCs;
	weights *= weights;

	// 0 / 0 -> NaN -> 1
	weights = min(weights, 1);

	if (targetCoC > 0)
		//weights *= saturate(1 - (targetCoC - abs(CoCs)) / abs(CoCs) * antileakFactor);
		weights *= saturate(1 + antileakFactor - targetCoC / abs(CoCs) * antileakFactor);

	return weights;
}

float3 Mix(float3 smooth, float3 sharp)
{
#if 1
	return lerp(smooth, sharp, .2f);
#else
	return sharp;
#endif
}

float3 DownsampleColor(float targetCoC, float dilatedCoC, float2 centerPoint, float2 cornerPoint)
{
	const float4 bilateralWeights[2][2] =
	{
		BilateralWeights(COCbuffer.GatherGreen(tapFilter, centerPoint, int2(-1, -1)), targetCoC, dilatedCoC),
		BilateralWeights(COCbuffer.GatherGreen(tapFilter, centerPoint, int2(+1, -1)), targetCoC, dilatedCoC),
		BilateralWeights(COCbuffer.GatherGreen(tapFilter, centerPoint, int2(-1, +1)), targetCoC, dilatedCoC),
		BilateralWeights(COCbuffer.GatherGreen(tapFilter, centerPoint, int2(+1, +1)), targetCoC, dilatedCoC)
	};

	float4 corners = 0, centerBlock = 0;
	FetchBlock(corners, centerBlock, int2(-1, -1), uint2(1, 1), cornerPoint, bilateralWeights[0][0]);
	FetchBlock(corners, centerBlock, int2(+1, -1), uint2(1, 0), cornerPoint, bilateralWeights[0][1]);
	FetchBlock(corners, centerBlock, int2(-1, +1), uint2(0, 1), cornerPoint, bilateralWeights[1][0]);
	FetchBlock(corners, centerBlock, int2(+1, +1), uint2(0, 0), cornerPoint, bilateralWeights[1][1]);

	// max(_, 0) to protect against possible 0 bilateral weights which translates to NaN after HDR decode
	corners.rgb = max(DecodeHDRExp(corners, cameraSettings.exposure), 0);
	centerBlock.rgb = max(DecodeHDRExp(centerBlock, cameraSettings.exposure), 0);

	return Mix(corners, centerBlock);
}

// for background hole filling
float DilateCoC(float CoC, float2 centerPoint)
{
	float dilatedCoC = abs(CoC);
	[unroll]
	for (int r = -holeFillingBlurBand; r <= +holeFillingBlurBand; r++)
		[unroll]
		for (int c = -holeFillingBlurBand; c <= +holeFillingBlurBand; c++)
		{
			const float dist = max(abs(r), abs(c));
			const float weight = 1 - dist / (holeFillingBlurBand + 1);	// (holeFillingBlurBand + 1 - dist) / (holeFillingBlurBand + 1)
			const float tapCoC = DOF::SelectCoC(COCbuffer.Gather(tapFilter, centerPoint, int2(c, r) * 2));
			dilatedCoC = max(dilatedCoC, lerp(abs(CoC), -tapCoC, weight));
		}
	return dilatedCoC;
}

/*
	scanline layout - probably not cache efficient
	tiled swizzling pattern can be faster - experiments on different GPUs wanted
*/
LensFlare::Source main(in uint flatPixelIdx : SV_VertexID)
{
	uint2 dstSize;
	dst.GetDimensions(dstSize.x, dstSize.y);
	const uint2 coord = { flatPixelIdx % dstSize.x, flatPixelIdx / dstSize.x };
	float2 center = coord + .5f;

	float2 centerPoint = center * 2, cornerPoint = centerPoint + .5f;
	{
		float2 srcSize;
		src.GetDimensions(srcSize.x, srcSize.y);
		centerPoint /= srcSize;
		cornerPoint /= srcSize;
		center /= dstSize;
	}

	const float CoC = DOF::SelectCoC(COCbuffer.Gather(tapFilter, centerPoint)), dilatedCoC = DilateCoC(CoC, centerPoint);

	// downsample to halfres with bilateral 5-tap Karis filter for DOF
	float3 color = DownsampleColor(CoC, dilatedCoC, centerPoint, cornerPoint);

	// store to halfres buffer for subsequent DOF splatting pass
	dst[coord] = float4(color, dilatedCoC);

	/*
	write directly to DOF blur layer to avoid rasterizing pixel-sized sprites
	do not blur sharp pixels with small CoC
	*/
	[branch]
	if (dilatedCoC + .5f <= Bokeh::R)
	{
		const float opacity = DOF::OpacityHalfres(dilatedCoC, cameraSettings.aperture);
		if (opacity)
		{
			color *= opacity;
			blurredLayers[uint3(coord, CoC > 0 ? DOF::BACKGROUND_NEAR_LAYER : DOF::FOREGROUND_FAR_LAYER)] = float4(color, opacity);
		}
	}

	// downsample to halfres with 13-tap partial Karis filter

	float4 block =
		src.SampleLevel(tapFilter, centerPoint, 0, int2(-1, -1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(+1, -1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(-1, +1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(+1, +1));

	color = DecodeHDRExp(block, .5f/*block weight*/ * cameraSettings.exposure);

	// shared taps
	const float4
		C = src.SampleLevel(tapFilter, centerPoint, 0),
		W = src.SampleLevel(tapFilter, centerPoint, 0, int2(-2, 0)),
		E = src.SampleLevel(tapFilter, centerPoint, 0, int2(+2, 0)),
		N = src.SampleLevel(tapFilter, centerPoint, 0, int2(0, -2)),
		S = src.SampleLevel(tapFilter, centerPoint, 0, int2(0, +2));

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(-2, -2)) + C + W + N;
	color += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(+2, -2)) + C + E + N;
	color += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(-2, +2)) + C + W + S;
	color += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(+2, +2)) + C + E + S;
	color += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	// transform 'center' UV -> NDC
	center *= 2;
	center -= 1;
	center.y = -center.y;

	LensFlare::Source flareSource =
	{
		center,
		cameraSettings.aperture.xx,
		cameraSettings.apertureRot,
		center * cameraSettings.aperture, length(center),
		color, Vignette(center)
	};

	flareSource.ext.x *= float(dstSize.y) / float(dstSize.x);

	// normalize
	const float dim = flareSource.ext.y * dstSize.y;
	flareSource.col.a *= LensFlare::normRebalance / (dim * dim);

	return flareSource;
}