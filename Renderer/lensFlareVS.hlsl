#include "lensFlare.hlsli"
#include "DOF.hlsli"
#include "Bokeh.hlsli"
#include "camera params.hlsli"
#include "HDR codec.hlsli"

SamplerState tapFilter : register(s0);
SamplerState bilateralTapSampler : register(s1);
SamplerState COCdownsampler : register(s2);
Texture2D src : register(t1);
Texture2D<float2> COCbuffer : register(t4);
RWTexture2D<float> dilatedCOCbuffer : register(u5);
RWTexture2D<float4> dst : register(u6);
RWTexture2DArray<float4> blurredLayers : register(u7);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

/*
* !: 3 is max supported with current implementation
* larger values causes out-of-range offsets in CoC sampling
* can fallback to DX11+ gather with programmable offsets to support larger values
*/
static const int holeFillingBlurBand = 3;
static const float shrinkFadeFactor = 3;	// controls how opacity fading affect CoC shrinking
static const float antileakFactor = 10;		// rcp(max leak percentage)

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

void OpacityPremultiply(inout float4 color/*HDR encoded*/, out float HDRnorm, in float CoC/*fullres*/, in float dilatedCoC/*halfres*/, in float foregroundOpacity/*signed*/)
{
	const float tapOpacity = DOF::OpacityHalfres(max(abs(CoC), dilatedCoC), cameraSettings.aperture);

	/*
	* split opacity into 2 components: intralayer + interlayer
	* intralayer leaks uncontrolled
	* interlayer limited up to remaining foreground opacity in order to achieve better temporal stability during background/foreground CoC jumps
	* 
	* assume foreground CoC is in slight-out-of-focus layer entirely (so blend far is 0 for background filed)
	* otherwise 'foregroundOpacity' should be pretty small so leak limitation no longer necessary
	*/
	
	// preserve intralayer leak
	const float intralayerLeak = foregroundOpacity > 0 ? 1 - DOF::BlendFar(CoC) : step(CoC, 0);
	float opacity = tapOpacity * intralayerLeak;

	// limited leak background onto foreground
	opacity += min(1 - saturate(abs(foregroundOpacity) + opacity), tapOpacity - opacity/*tapOpacity * (1 - intralayerLeak)*/);
	
	color.rgb = max(DecodeHDRExp(color, cameraSettings.exposure * opacity), 0);
	color.a = opacity;
	EncodeHDRPremultiplied(color, HDRnorm);
}

float4 BilateralWeights(float4 CoCBlock, float targetCoC)
{
	float4 weights = 1;

	if (targetCoC > 0)
		//weights *= saturate(1 - (targetCoC - abs(CoCBlock)) / abs(CoCBlock) * antileakFactor);
		weights *= saturate(1 + antileakFactor - targetCoC / abs(CoCBlock) * antileakFactor);

	return weights;
}

void FetchBlock(inout float4 cornerColor, inout float cornerNorm, inout float4 centerColor, inout float centerNorm, uniform int2 offset/*right bottom*/, uniform uint2 centerIdx, in float targetCoC, in float dilatedCoC, in float foregroundOpacity, in float2 centerPoint, in float2 cornerPoint)
{
	const float4 CoCBlock = COCbuffer.GatherGreen(COCdownsampler, centerPoint, offset);
	float4 colorBlock[2][2] =
	{
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(1, 1)),
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(0, 1)),
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(1, 0)),
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(0, 0))
	};
	float HDRnormBlock[2][2];
	OpacityPremultiply(colorBlock[0][0], HDRnormBlock[0][0], UnpackGatherBlock(CoCBlock, uint2(0, 0)), dilatedCoC, foregroundOpacity);
	OpacityPremultiply(colorBlock[0][1], HDRnormBlock[0][1], UnpackGatherBlock(CoCBlock, uint2(0, 1)), dilatedCoC, foregroundOpacity);
	OpacityPremultiply(colorBlock[1][0], HDRnormBlock[1][0], UnpackGatherBlock(CoCBlock, uint2(1, 0)), dilatedCoC, foregroundOpacity);
	OpacityPremultiply(colorBlock[1][1], HDRnormBlock[1][1], UnpackGatherBlock(CoCBlock, uint2(1, 1)), dilatedCoC, foregroundOpacity);

	const float4 bilateralWeights = BilateralWeights(CoCBlock, targetCoC);
	colorBlock[0][0] *= UnpackGatherBlock(bilateralWeights, uint2(0, 0));
	colorBlock[0][1] *= UnpackGatherBlock(bilateralWeights, uint2(0, 1));
	colorBlock[1][0] *= UnpackGatherBlock(bilateralWeights, uint2(1, 0));
	colorBlock[1][1] *= UnpackGatherBlock(bilateralWeights, uint2(1, 1));
	HDRnormBlock[0][0] *= UnpackGatherBlock(bilateralWeights, uint2(0, 0));
	HDRnormBlock[0][1] *= UnpackGatherBlock(bilateralWeights, uint2(0, 1));
	HDRnormBlock[1][0] *= UnpackGatherBlock(bilateralWeights, uint2(1, 0));
	HDRnormBlock[1][1] *= UnpackGatherBlock(bilateralWeights, uint2(1, 1));

	centerColor += colorBlock[centerIdx.x][centerIdx.y];
	centerNorm += HDRnormBlock[centerIdx.x][centerIdx.y];
	cornerColor += colorBlock[0][0] + colorBlock[0][1] + colorBlock[1][0] + colorBlock[1][1];
	cornerNorm += HDRnormBlock[0][0] + HDRnormBlock[0][1] + HDRnormBlock[1][0] + HDRnormBlock[1][1];
}

float4 Mix(float4 smooth, float4 sharp)
{
#if 1
	return lerp(smooth, sharp, .2f);
#else
	return sharp;
#endif
}

float4 DownsampleColor(float targetCoC, float dilatedCoC, float2 centerPoint, float2 cornerPoint)
{
	const float foregroundOpacity = saturate(DOF::Opacity(targetCoC, cameraSettings.aperture)) * sign(targetCoC);
	float4 cornersColor = 0, centerBlockColor = 0;
	float cornersNorm = 0, centerBlockNorm = 0;
	FetchBlock(cornersColor, cornersNorm, centerBlockColor, centerBlockNorm, int2(-1, -1), uint2(1, 1), targetCoC, dilatedCoC, foregroundOpacity, centerPoint, cornerPoint);
	FetchBlock(cornersColor, cornersNorm, centerBlockColor, centerBlockNorm, int2(+1, -1), uint2(1, 0), targetCoC, dilatedCoC, foregroundOpacity, centerPoint, cornerPoint);
	FetchBlock(cornersColor, cornersNorm, centerBlockColor, centerBlockNorm, int2(-1, +1), uint2(0, 1), targetCoC, dilatedCoC, foregroundOpacity, centerPoint, cornerPoint);
	FetchBlock(cornersColor, cornersNorm, centerBlockColor, centerBlockNorm, int2(+1, +1), uint2(0, 0), targetCoC, dilatedCoC, foregroundOpacity, centerPoint, cornerPoint);

	DecodeHDRPremultiplied(cornersColor, cornersNorm);
	DecodeHDRPremultiplied(centerBlockColor, centerBlockNorm);

	return Mix(cornersColor, centerBlockColor);
}

// for background hole filling
float DilateCoC(float CoC, float2 centerPoint)
{
	float shrinkedCoC = abs(CoC), dilatedCoC = 0, dilatedWeight = 0;
	[unroll]
	for (int r = -holeFillingBlurBand; r <= +holeFillingBlurBand; r++)
		[unroll]
		for (int c = -holeFillingBlurBand; c <= +holeFillingBlurBand; c++)
		{
			const float tapCoC = DOF::DownsampleCoC(COCbuffer, COCdownsampler, centerPoint, int2(c, r) * 2);
			if (tapCoC < CoC)
			{
#if 1
				const float dist = max(abs(r), abs(c)) - 1;					// max 'holeFillingBlurBand - 1'
				float weight = saturate(1 - dist / holeFillingBlurBand);	// (holeFillingBlurBand - dist) / holeFillingBlurBand
#else
				const float dist = max(abs(r), abs(c));						// max 'holeFillingBlurBand'
				float weight = 1 - dist / (holeFillingBlurBand + 1);		// (holeFillingBlurBand + 1 - dist) / (holeFillingBlurBand + 1)
#endif
				const float weightedTap = weight * -tapCoC;
				[flatten]
				if (weightedTap > dilatedCoC)
				{
					dilatedCoC = weightedTap;
					dilatedWeight = weight;
				}
				if (tapCoC > 0)
				{
					// prevent shrinking across layers\
					can also opt to shrink in near layer only - use '1 - DOF::BlendFar(CoC)' factor instead (which is 'CoC' blend near factor)
					weight *= 1 - (DOF::BlendFar(CoC) - DOF::BlendFar(tapCoC));
					weight *= saturate(DOF::OpacityHalfres(tapCoC, cameraSettings.aperture) * shrinkFadeFactor);
					shrinkedCoC = min(shrinkedCoC, lerp(abs(CoC), abs(tapCoC), weight));
				}
			}
		}
	return max(shrinkedCoC, shrinkedCoC * (1 - dilatedWeight) + dilatedCoC);
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

	const float CoC = DOF::DownsampleCoC(COCbuffer, COCdownsampler, centerPoint), dilatedCoC = DilateCoC(CoC, centerPoint);

	// downsample to halfres with bilateral 5-tap Karis filter for DOF
	float4 color = DownsampleColor(CoC, dilatedCoC, centerPoint, cornerPoint);

	/*
	write directly to DOF blur layer to avoid rasterizing pixel-sized sprites
	do not blur sharp pixels with small CoC
	*/
	[branch]
	if (color.a && dilatedCoC + .5f <= Bokeh::R)
	{
		blurredLayers[uint3(coord, CoC > 0 ? DOF::BACKGROUND_NEAR_LAYER : DOF::FOREGROUND_FAR_LAYER)] = color;
		color = 0;	// disable rasterization in DOF GS
	}

	// store to halfres buffers for subsequent DOF splatting pass
	dilatedCOCbuffer[coord] = dilatedCoC;
	dst[coord] = color;

	// downsample to halfres with 13-tap partial Karis filter

	float4 block =
		src.SampleLevel(tapFilter, centerPoint, 0, int2(-1, -1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(+1, -1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(-1, +1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(+1, +1));

	color.rgb = DecodeHDRExp(block, .5f/*block weight*/ * cameraSettings.exposure);

	// shared taps
	const float4
		C = src.SampleLevel(tapFilter, centerPoint, 0),
		W = src.SampleLevel(tapFilter, centerPoint, 0, int2(-2, 0)),
		E = src.SampleLevel(tapFilter, centerPoint, 0, int2(+2, 0)),
		N = src.SampleLevel(tapFilter, centerPoint, 0, int2(0, -2)),
		S = src.SampleLevel(tapFilter, centerPoint, 0, int2(0, +2));

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(-2, -2)) + C + W + N;
	color.rgb += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(+2, -2)) + C + E + N;
	color.rgb += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(-2, +2)) + C + W + S;
	color.rgb += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, centerPoint, 0, int2(+2, +2)) + C + E + S;
	color.rgb += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

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
		color.rgb, Vignette(center)
	};

	flareSource.ext.x *= float(dstSize.y) / float(dstSize.x);

	// normalize
	const float dim = flareSource.ext.y * dstSize.y;
	flareSource.col.a *= LensFlare::normRebalance / (dim * dim);

	return flareSource;
}