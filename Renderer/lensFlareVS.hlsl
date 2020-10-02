#include "lensFlare.hlsli"
#include "DOF.hlsli"
#include "Bokeh.hlsli"
#include "camera params.hlsli"
#include "HDR codec.hlsli"

SamplerState tapFilter : register(s0);
SamplerState bilateralTapSampler : register(s1);
SamplerState COCdownsampler : register(s2);
Texture2D src : register(t1);
Texture2D<float> DOFopacityBuffer : register(t4);
Texture2D<float> COCbuffer : register(t5);
RWTexture2D<float> dilatedCOCbuffer : register(u6);
RWTexture2D<float4> dst : register(u7);
RWTexture2DArray<float4> blurredLayers : register(u8);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

/*
* !: 3 is max supported with current implementation
* larger values causes out-of-range offsets in CoC sampling
* can fallback to DX11+ gather with programmable offsets to support larger values
*/
static const int holeFillingBlurBand = 3;
static const float shrinkFadeFactor = 3;	// controls how opacity fading affect CoC shrinking
static const float antileakFactor = 10;		// rcp(max leak percentage)

inline float Vignette(float2 dir)
{
	const float fade = saturate(1.8f - dot(dir, dir));
	return fade * fade;
}

inline uint FlattenGatherBlockIdx(uniform uint2 idx)
{
	static const uint remapLUT[2][2] =
	{
		3, 2,
		0, 1
	};
	return remapLUT[idx.x][idx.y];
}

inline float UnpackGatherBlock(in float4 block, uniform uint2 idx)
{
	return block[FlattenGatherBlockIdx(idx)];
}

void OpacityPremultiply(inout float4 color/*HDR encoded*/, out float HDRnorm,
	in float tapCoC/*fullres*/, in float tapOpacity/*fullres*/, in float dilatedOpacity/*halfres*/, in float foregroundOpacity/*signed*/, in float bilateralWeight)
{
	/*
	* split opacity into 2 components: intralayer + interlayer
	* intralayer leaks uncontrolled
	* interlayer limited up to remaining foreground opacity in order to achieve better temporal stability during background/foreground CoC jumps
	* 
	* assume foreground CoC is in slight-out-of-focus layer entirely (so blend near is 1 for background filed)
	* otherwise 'foregroundOpacity' should be pretty small so leak limitation no longer necessary
	*/
	
	// preserve intralayer leak
	const float intralayerLeak = foregroundOpacity > 0 ? DOF::BlendNear(DOF::CoC(tapOpacity, cameraSettings.aperture)) : step(tapCoC, 0);
	tapOpacity = min(DOF::HalfresOpacity(tapOpacity), dilatedOpacity);	// 'tapOpacity' now in halfres units
	float opacity = tapOpacity * intralayerLeak;

	// limited leak background onto foreground
	opacity += min(1 - saturate(abs(foregroundOpacity) + opacity), tapOpacity - opacity/*tapOpacity * (1 - intralayerLeak)*/);

	opacity *= bilateralWeight;
	
	color.rgb = max(DecodeHDRExp(color, cameraSettings.exposure * opacity), 0);
	color.a = opacity;
	EncodeHDRPremultiplied(color, HDRnorm);
}

float4 BilateralWeights(float4 CoCBlock, float targetCoC)
{
	float4 weights = 1;

	// should be 'antileakFactor >= 1' in order to 'weights = 0' for 'CoCBlock < 0' case
	if (targetCoC > 0)
		weights = saturate(1 - (targetCoC - CoCBlock) / abs(CoCBlock) * antileakFactor);

	return weights;
}

void FetchBlock(inout float4 cornerColor, inout float cornerNorm, inout float4 centerColor, inout float centerNorm, uniform int2 offset/*right bottom*/, uniform uint2 centerIdx,
	in float4 CoCBlock, in float dilatedOpacity, in float foregroundOpacity, in float4 bilateralWeightsBlock, in float2 centerPoint, in float2 cornerPoint)
{
	const float4 opacityBlock = DOFopacityBuffer.Gather(COCdownsampler, centerPoint, offset);	// in fullres units here
	float4 colorBlock[2][2] =
	{
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(1, 1)),
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(0, 1)),
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(1, 0)),
		src.SampleLevel(bilateralTapSampler, cornerPoint, 0, offset - int2(0, 0))
	};
	float HDRnormBlock[2][2];
	OpacityPremultiply(colorBlock[0][0], HDRnormBlock[0][0], UnpackGatherBlock(CoCBlock, uint2(0, 0)), UnpackGatherBlock(opacityBlock, uint2(0, 0)), dilatedOpacity, foregroundOpacity, UnpackGatherBlock(bilateralWeightsBlock, uint2(0, 0)));
	OpacityPremultiply(colorBlock[0][1], HDRnormBlock[0][1], UnpackGatherBlock(CoCBlock, uint2(0, 1)), UnpackGatherBlock(opacityBlock, uint2(0, 1)), dilatedOpacity, foregroundOpacity, UnpackGatherBlock(bilateralWeightsBlock, uint2(0, 1)));
	OpacityPremultiply(colorBlock[1][0], HDRnormBlock[1][0], UnpackGatherBlock(CoCBlock, uint2(1, 0)), UnpackGatherBlock(opacityBlock, uint2(1, 0)), dilatedOpacity, foregroundOpacity, UnpackGatherBlock(bilateralWeightsBlock, uint2(1, 0)));
	OpacityPremultiply(colorBlock[1][1], HDRnormBlock[1][1], UnpackGatherBlock(CoCBlock, uint2(1, 1)), UnpackGatherBlock(opacityBlock, uint2(1, 1)), dilatedOpacity, foregroundOpacity, UnpackGatherBlock(bilateralWeightsBlock, uint2(1, 1)));

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
	const float4 CoCsFootprint[2][2] =
	{
		COCbuffer.Gather(tapFilter, centerPoint, int2(-1, -1)),
		COCbuffer.Gather(tapFilter, centerPoint, int2(+1, -1)),
		COCbuffer.Gather(tapFilter, centerPoint, int2(-1, +1)),
		COCbuffer.Gather(tapFilter, centerPoint, int2(+1, +1))
	};

	float4 bilateralWeights[2][2] =
	{
		BilateralWeights(CoCsFootprint[0][0], targetCoC),
		BilateralWeights(CoCsFootprint[0][1], targetCoC),
		BilateralWeights(CoCsFootprint[1][0], targetCoC),
		BilateralWeights(CoCsFootprint[1][1], targetCoC)
	};

	// normalize weights
	const float4 norm4 = bilateralWeights[0][0] + bilateralWeights[0][1] + bilateralWeights[1][0] + bilateralWeights[1][1];
	const float2 norm2 = norm4.xy + norm4.zw;
	const float1 norm1 = norm2.x + norm2.y;
	const float centerNormCompensate = 4 - norm1 * .25f;	// (16 - norm1) / 4
	bilateralWeights[0][0][FlattenGatherBlockIdx(uint2(1, 1))] += centerNormCompensate;
	bilateralWeights[0][1][FlattenGatherBlockIdx(uint2(1, 0))] += centerNormCompensate;
	bilateralWeights[1][0][FlattenGatherBlockIdx(uint2(0, 1))] += centerNormCompensate;
	bilateralWeights[1][1][FlattenGatherBlockIdx(uint2(0, 0))] += centerNormCompensate;

	const float
		dilatedOpacity = DOF::OpacityHalfres(dilatedCoC, cameraSettings.aperture),
		foregroundOpacity = DOF::HalfresOpacity(DOF::Opacity(targetCoC, cameraSettings.aperture)) * sign(targetCoC);
	float4 cornersColor = 0, centerBlockColor = 0;
	float cornersNorm = 0, centerBlockNorm = 0;
	FetchBlock(cornersColor, cornersNorm, centerBlockColor, centerBlockNorm, int2(-1, -1), uint2(1, 1), CoCsFootprint[0][0], dilatedOpacity, foregroundOpacity, bilateralWeights[0][0], centerPoint, cornerPoint);
	FetchBlock(cornersColor, cornersNorm, centerBlockColor, centerBlockNorm, int2(+1, -1), uint2(1, 0), CoCsFootprint[0][1], dilatedOpacity, foregroundOpacity, bilateralWeights[0][1], centerPoint, cornerPoint);
	FetchBlock(cornersColor, cornersNorm, centerBlockColor, centerBlockNorm, int2(-1, +1), uint2(0, 1), CoCsFootprint[1][0], dilatedOpacity, foregroundOpacity, bilateralWeights[1][0], centerPoint, cornerPoint);
	FetchBlock(cornersColor, cornersNorm, centerBlockColor, centerBlockNorm, int2(+1, +1), uint2(0, 0), CoCsFootprint[1][1], dilatedOpacity, foregroundOpacity, bilateralWeights[1][1], centerPoint, cornerPoint);

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
				const float dist = max(abs(r), abs(c)) - 1;					// max 'holeFillingBlurBand - 1'
				float weight = saturate(1 - dist / holeFillingBlurBand);	// (holeFillingBlurBand - dist) / holeFillingBlurBand
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
					can also opt to shrink in near layer only - use 'DOF::BlendNear(CoC)' factor instead
					weight *= 1 - (DOF::BlendNear(tapCoC) - DOF::BlendNear(CoC));
					weight *= saturate(DOF::OpacityHalfres(tapCoC, cameraSettings.aperture) * shrinkFadeFactor);
					shrinkedCoC = min(shrinkedCoC, lerp(abs(CoC), abs(tapCoC), weight));
				}
			}
		}
	return max(shrinkedCoC, shrinkedCoC * (1 - dilatedWeight) + dilatedCoC);
}

// downsample + COC dilation/shrinking + pixel-sized splats
void PrepareDOF(uint2 coord, float2 centerPoint, float2 cornerPoint)
{

	const float CoC = DOF::DownsampleCoC(COCbuffer, COCdownsampler, centerPoint), dilatedCoC = DilateCoC(CoC, centerPoint);

	// downsample to halfres with bilateral 5-tap Karis filter for DOF
	float4 color = DownsampleColor(CoC, dilatedCoC, centerPoint, cornerPoint);

	/*
	write directly to DOF blur layer to avoid rasterizing pixel-sized sprites
	do not blur sharp pixels with small CoC
	'dilatedCoC <= Bokeh::R * 2 - 1' is optimized equivalent for 'dilatedCoC * .5f + .5f <= Bokeh::R' (rely on constant folding)
	*/
	[branch]
	if (color.a && dilatedCoC <= Bokeh::R * 2/*to fullres*/ - 1/*half pixel in fullres units*/)
	{
		blurredLayers[uint3(coord, CoC > 0 ? DOF::BACKGROUND_NEAR_LAYER : DOF::FOREGROUND_FAR_LAYER)] = color;
		color = 0;	// disable rasterization in DOF GS
	}

	// store to halfres buffers for subsequent DOF splatting pass
	dilatedCOCbuffer[coord] = dilatedCoC;
	dst[coord] = color;
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
	PrepareDOF(coord, centerPoint, cornerPoint);

	// downsample to halfres with 13-tap partial Karis filter

	float4 block =
		src.SampleLevel(tapFilter, centerPoint, 0, int2(-1, -1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(+1, -1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(-1, +1)) +
		src.SampleLevel(tapFilter, centerPoint, 0, int2(+1, +1));

	float3 color = DecodeHDRExp(block, .5f/*block weight*/ * cameraSettings.exposure);

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

	const float invApect = float(dstSize.y) / float(dstSize.x);

	float2 dir = center;
	dir.y *= invApect;	// align by width, squash vertically

	LensFlare::Source flareSource =
	{
		center,
		cameraSettings.aperture.xx,
		cameraSettings.apertureRot,
		dir * cameraSettings.aperture, length(dir),
		color, Vignette(dir)
	};

	flareSource.ext.x *= invApect;

	// normalize
	const float dim = flareSource.ext.y * dstSize.y;
	flareSource.col.a *= LensFlare::normRebalance / (dim * dim);

	return flareSource;
}