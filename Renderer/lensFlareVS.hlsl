#include "lensFlare.hlsli"
#include "DOF.hlsli"
#include "Bokeh.hlsli"
#include "camera params.hlsli"
#include "HDR codec.hlsli"

#define BILATERAL_DOWNSAMPLE 1
#define MSAA_BILATERAL_WEIGHTING 0
#define FULLRES_COC_INFLATION 0
#define DIST_BASED_BLEED 0

SamplerState tapFilter : register(s0);
SamplerState bilateralTapSampler : register(s1);
#if MSAA_BILATERAL_WEIGHTING || DIST_BASED_BLEED
Texture2DMS<float> ZBuffer : register(t0);
#endif
Texture2D src : register(t1);
Texture2D<float2> COCbuffer : register(t4);
RWTexture2D<float4> dst : register(u5);
RWTexture2DArray<float4> blurredLayers : register(u6);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

static const int holeFillingBlurBand = 4;
static const float antileakFactor = 10;	// rcp(max leak percentage)
static const float2 bleedTransitionRange = float2(.1f, .6f);

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

float Bilerp(float4 block, float2 weights)
{
	const float2 row = lerp(block.wz, block.xy, weights.y);
	return lerp(row[0], row[1], weights.x);
}

float4 Bilerp(float4 block[2][2], float2 weights)
{
	const float4 row[2] =
	{
		lerp(block[0][0], block[1][0], weights.y),
		lerp(block[0][1], block[1][1], weights.y)
	};
	return lerp(row[0], row[1], weights.x);
}

void FetchBlock(inout float4 corner, out float4 center, uniform int2 offset/*right bottom*/, uniform uint2 centerIdx, in float2 cornerPoint, in float2 bilerpWeights, in float4 bilateralWeights)
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
	center = block[centerIdx.x][centerIdx.y];
	corner += Bilerp(block, bilerpWeights) * .25f;
}

float4 BilateralWeights(float4 CoCs, float targetCoC, float dilatedCoC)
{
	float4 weights = dilatedCoC / CoCs;
	weights *= weights;

	// 0 / 0 -> NaN -> 1
	weights = min(weights, 1);

	if (targetCoC > 0)
#if 0
		weights *= min(abs(CoCs) / targetCoC, 1);
#else
		//weights *= saturate(1 - (targetCoC - abs(CoCs)) / abs(CoCs) * antileakFactor);
		weights *= saturate(1 + antileakFactor - targetCoC / abs(CoCs) * antileakFactor);
#endif

	return weights;
}

#if MSAA_BILATERAL_WEIGHTING
float4 MSAABilateralWeights(uniform int2 offset/*right bottom*/, in float2 cornerPoint, in float targetCoC);
#endif

float3 Mix(float3 smooth, float3 sharp)
{
#if 1
	return lerp(smooth, sharp, .2f);
#else
	return sharp;
#endif
}

#if BILATERAL_DOWNSAMPLE
float DownsampleCoC(in float2 centerPoint, uniform int2 offset = 0)
{
#if 0
	const float4 centerCoCBlock =
	{
		UnpackGatherBlock(CoCsFootprint[1][0], uint2(0, 1)),
		UnpackGatherBlock(CoCsFootprint[1][1], uint2(0, 0)),
		UnpackGatherBlock(CoCsFootprint[0][1], uint2(1, 0)),
		UnpackGatherBlock(CoCsFootprint[0][0], uint2(1, 1))
	};
	float CoC = Bilerp(centerCoCBlock, bilerpWeights);
#if 1
	CoC += Bilerp(CoCsFootprint[0][0], bilerpWeights);
	CoC += Bilerp(CoCsFootprint[0][1], bilerpWeights);
	CoC += Bilerp(CoCsFootprint[1][0], bilerpWeights);
	CoC += Bilerp(CoCsFootprint[1][1], bilerpWeights);
	CoC *= .2f;
#endif
#elif 0
	const float CoC = DOF::SelectCoC(float4(
		UnpackGatherBlock(CoCsFootprint[0][0], uint2(1, 1)), UnpackGatherBlock(CoCsFootprint[0][1], uint2(1, 0)),
		UnpackGatherBlock(CoCsFootprint[1][0], uint2(0, 1)), UnpackGatherBlock(CoCsFootprint[1][1], uint2(0, 0))));
#elif 0
	float4 CoCs =			COCbuffer.Gather(tapFilter, centerPoint, offset + int2(-1, -1));
	DOF::SelectCoC(CoCs,	COCbuffer.Gather(tapFilter, centerPoint, offset + int2(+1, -1)));
	DOF::SelectCoC(CoCs,	COCbuffer.Gather(tapFilter, centerPoint, offset + int2(-1, +1)));
	DOF::SelectCoC(CoCs,	COCbuffer.Gather(tapFilter, centerPoint, offset + int2(+1, +1)));
	const float CoC = DOF::SelectCoC(CoCs);
#else
	const float CoC = DOF::SelectCoC(COCbuffer.Gather(tapFilter, centerPoint, offset));
#endif
	return CoC;
}
#else
float DownsampleCoC(in float2 center)
{
	/*
	select min CoC to prevent bleeding
	can also try max Z with bilateral color filter
	*/
	float4 CoCs =			COCbuffer.Gather(tapFilter, center, int2(-1, -1));
	DOF::SelectCoC(CoCs,	COCbuffer.Gather(tapFilter, center, int2(+1, -1)));
	DOF::SelectCoC(CoCs,	COCbuffer.Gather(tapFilter, center, int2(-1, +1)));
	DOF::SelectCoC(CoCs,	COCbuffer.Gather(tapFilter, center, int2(+1, +1)));
	return DOF::SelectCoC(CoCs);
}
#endif

#if BILATERAL_DOWNSAMPLE
float3 DownsampleColor(in float targetCoC, in float dilatedCoC, in float2 centerPoint, in float2 cornerPoint, in float2 bilerpWeights, uniform int2 offset = 0)
{
#if 1
	float4 CoCsFootprint[2][2] =
	{
		COCbuffer.GatherGreen(tapFilter, centerPoint, offset + int2(-1, -1)),
		COCbuffer.GatherGreen(tapFilter, centerPoint, offset + int2(+1, -1)),
		COCbuffer.GatherGreen(tapFilter, centerPoint, offset + int2(-1, +1)),
		COCbuffer.GatherGreen(tapFilter, centerPoint, offset + int2(+1, +1))
	};
#else
	float4 CoCsFootprint[2][2] =
	{
		COCbuffer.Gather(tapFilter, centerPoint, offset + int2(-1, -1)),
		COCbuffer.Gather(tapFilter, centerPoint, offset + int2(+1, -1)),
		COCbuffer.Gather(tapFilter, centerPoint, offset + int2(-1, +1)),
		COCbuffer.Gather(tapFilter, centerPoint, offset + int2(+1, +1))
	};
#endif

#if MSAA_BILATERAL_WEIGHTING
	CoCsFootprint[0][0] = MSAABilateralWeights(offset + int2(-1, -1), cornerPoint, targetCoC);
	CoCsFootprint[0][1] = MSAABilateralWeights(offset + int2(+1, -1), cornerPoint, targetCoC);
	CoCsFootprint[1][0] = MSAABilateralWeights(offset + int2(-1, +1), cornerPoint, targetCoC);
	CoCsFootprint[1][1] = MSAABilateralWeights(offset + int2(+1, +1), cornerPoint, targetCoC);
#else
	CoCsFootprint[0][0] = BilateralWeights(CoCsFootprint[0][0], targetCoC, dilatedCoC);
	CoCsFootprint[0][1] = BilateralWeights(CoCsFootprint[0][1], targetCoC, dilatedCoC);
	CoCsFootprint[1][0] = BilateralWeights(CoCsFootprint[1][0], targetCoC, dilatedCoC);
	CoCsFootprint[1][1] = BilateralWeights(CoCsFootprint[1][1], targetCoC, dilatedCoC);
#endif

	float4 color = 0, centerBlock[2][2];
	FetchBlock(color, centerBlock[0][0], offset + int2(-1, -1), uint2(1, 1), cornerPoint, bilerpWeights, CoCsFootprint[0][0]);
	FetchBlock(color, centerBlock[0][1], offset + int2(+1, -1), uint2(1, 0), cornerPoint, bilerpWeights, CoCsFootprint[0][1]);
	FetchBlock(color, centerBlock[1][0], offset + int2(-1, +1), uint2(0, 1), cornerPoint, bilerpWeights, CoCsFootprint[1][0]);
	FetchBlock(color, centerBlock[1][1], offset + int2(+1, +1), uint2(0, 0), cornerPoint, bilerpWeights, CoCsFootprint[1][1]);
	color.rgb = max(DecodeHDRExp(color, cameraSettings.exposure), 0);
	const float3 centerContrib = max(DecodeHDRExp(Bilerp(centerBlock, bilerpWeights), cameraSettings.exposure), 0);

	return Mix(color, centerContrib);
}
#else
float3 DownsampleColor(in float2 center)
{
	float4 color = 0;
	color += src.SampleLevel(tapFilter, center, 0, int2(-1, -1)) * .25f;
	color += src.SampleLevel(tapFilter, center, 0, int2(+1, -1)) * .25f;
	color += src.SampleLevel(tapFilter, center, 0, int2(-1, +1)) * .25f;
	color += src.SampleLevel(tapFilter, center, 0, int2(+1, +1)) * .25f;
	color.rgb = DecodeHDRExp(color, cameraSettings.exposure);
	const float3 centerContrib = DecodeHDRExp(src.SampleLevel(tapFilter, center, 0), cameraSettings.exposure);

	return Mix(color, centerContrib);
}
#endif

// for background hole filling
float DilateCoC(float CoC, float2 centerPoint)
{
	float dilatedCoC = abs(CoC);
	[unroll]
	for (int r = -holeFillingBlurBand; r <= +holeFillingBlurBand; r++)
		[unroll]
		for (int c = -holeFillingBlurBand; c <= +holeFillingBlurBand; c++)
		{
#if FULLRES_COC_INFLATION
#error not implemented
#else
			const float dist = max(abs(r), abs(c));
			const float weight = 1 - dist / (holeFillingBlurBand + 1);	// (holeFillingBlurBand + 1 - dist) / (holeFillingBlurBand + 1)
			const float tapCoC = DOF::SelectCoC(COCbuffer.Gather(tapFilter, centerPoint, int2(c, r) * 2));
			dilatedCoC = max(dilatedCoC, lerp(abs(CoC), -tapCoC, weight));
#endif
		}
	return dilatedCoC;
}

inline float BleedAmount(float foregroundCoC, float backgroundCoC)
{
	const float diff = max((backgroundCoC - foregroundCoC) / abs(foregroundCoC), 0);

	// or just lerp ?
	return smoothstep(bleedTransitionRange[0], bleedTransitionRange[1], diff);
}

#if DIST_BASED_BLEED
inline void BleedSample(inout float bleed, in float backgroundCoC, in int2 fetchPoint, uniform int2 fetchOffset, uniform int2 pixelOffset, uniform int2 dir, in float2 planeDist, in uint sampleIdx)
{
	planeDist += pixelOffset;
	planeDist *= dir * .5f;
	const float dist = max(planeDist.x, planeDist.y);
	const float sampleCoC = cameraSettings.COC(ZBuffer.Load(fetchPoint, sampleIdx, fetchOffset + pixelOffset));
	const float sampleBleed = BleedAmount(sampleCoC, backgroundCoC) * (1 - dist);
	bleed = max(bleed, sampleBleed);
}
#endif

#if BILATERAL_DOWNSAMPLE
void ProcessBleedNeighbor(inout float outBleed, inout float2 normFactors, inout float bleedCoC, in float foregroundCoC, in float backgroundCoC, in float2 centerPoint, uniform int2 offset, uniform int2 dir)
{
	offset += dir * 2;
	const bool helperNeighbor = any(offset);

	if (helperNeighbor)
		foregroundCoC = DownsampleCoC(centerPoint, offset);

	if (foregroundCoC < backgroundCoC)
	{
#if DIST_BASED_BLEED
		uint2 size;
		uint MSAA;
		ZBuffer.GetDimensions(size.x, size.y, MSAA);
		const int2 fetchPoint = centerPoint * size + .5f;
		float bleed = 0;
		for (uint sampleIdx = 0; sampleIdx < MSAA; sampleIdx++)
		{
			const float2 planeDist = dir + .5f/*2 * dir - (dir - .5f)*/ + ZBuffer.GetSamplePosition(sampleIdx);
			BleedSample(bleed, backgroundCoC, fetchPoint, offset, int2(-1, -1), dir, planeDist, sampleIdx);
			BleedSample(bleed, backgroundCoC, fetchPoint, offset, int2(-0, -1), dir, planeDist, sampleIdx);
			BleedSample(bleed, backgroundCoC, fetchPoint, offset, int2(-1, -0), dir, planeDist, sampleIdx);
			BleedSample(bleed, backgroundCoC, fetchPoint, offset, int2(-0, -0), dir, planeDist, sampleIdx);
		}
#else
		const float avgCoC = dot(COCbuffer.GatherGreen(tapFilter, centerPoint, offset), (float4).25f);
		const float bleed = saturate(BleedAmount(foregroundCoC, backgroundCoC) * (avgCoC - backgroundCoC) / (foregroundCoC - backgroundCoC));
#endif

		normFactors.x += bleed;
		normFactors.y = max(normFactors.y, bleed);

		if (helperNeighbor)
			DOF::SelectCoC(bleedCoC, foregroundCoC);
		else
			outBleed = bleed;
	}
}

void BleedBackground(inout float3 foreground, in float foregroundCoC, in float2 centerPoint, in float2 cornerPoint, in float2 bilerpWeights, uniform int2 offset)
{
	const float backgroundCoC = DownsampleCoC(centerPoint, offset);

	[branch]
	if (foregroundCoC < backgroundCoC)
	{
		float bleed;
		float2 normFactors = 0;
		float bleedCoC = foregroundCoC;
		ProcessBleedNeighbor(bleed, normFactors, bleedCoC, foregroundCoC, backgroundCoC, centerPoint, offset, int2(-1,  0));
		ProcessBleedNeighbor(bleed, normFactors, bleedCoC, foregroundCoC, backgroundCoC, centerPoint, offset, int2( 0, -1));
		ProcessBleedNeighbor(bleed, normFactors, bleedCoC, foregroundCoC, backgroundCoC, centerPoint, offset, int2(+1,  0));
		ProcessBleedNeighbor(bleed, normFactors, bleedCoC, foregroundCoC, backgroundCoC, centerPoint, offset, int2( 0, +1));
		ProcessBleedNeighbor(bleed, normFactors, bleedCoC, foregroundCoC, backgroundCoC, centerPoint, offset, int2(-1, -1));
		ProcessBleedNeighbor(bleed, normFactors, bleedCoC, foregroundCoC, backgroundCoC, centerPoint, offset, int2(+1, -1));
		ProcessBleedNeighbor(bleed, normFactors, bleedCoC, foregroundCoC, backgroundCoC, centerPoint, offset, int2(+1, +1));
		ProcessBleedNeighbor(bleed, normFactors, bleedCoC, foregroundCoC, backgroundCoC, centerPoint, offset, int2(-1, +1));

		bleed *= max(normFactors.y / normFactors.x, 0);
		foreground += DownsampleColor(bleedCoC, bleedCoC/*?*/, centerPoint, cornerPoint, bilerpWeights, offset) * bleed;
	}
}
#endif

/*
	scanline layout - probably not cache efficient
	tiled swizzling pattern can be faster - experiments on different GPUs wanted
*/
LensFlare::Source main(in uint flatPixelIdx : SV_VertexID)
{
	uint2 dstSize;
	dst.GetDimensions(dstSize.x, dstSize.y);
	const uint2 coord = { flatPixelIdx % dstSize.x, flatPixelIdx / dstSize.x };
	float2 center = (coord + .5f) / dstSize;

#if BILATERAL_DOWNSAMPLE
	float2 srcSize;
	src.GetDimensions(srcSize.x, srcSize.y);
	float2 centerPoint;
	const float2 bilerpWeights = modf(center * srcSize + .5f, centerPoint);
	float2 cornerPoint = centerPoint + .5f;
	centerPoint /= srcSize;
	cornerPoint /= srcSize;
	const float CoC = DownsampleCoC(centerPoint), dilatedCoC = DilateCoC(CoC, centerPoint);
#else
	const float CoC = DownsampleCoC(center);
#endif

	// downsample to halfres with 5-tap Karis filter for both lens flare and DOF
#if BILATERAL_DOWNSAMPLE
	float4 color = float4(DownsampleColor(CoC, dilatedCoC, centerPoint, cornerPoint, bilerpWeights), 1);
#if 0
	BleedBackground(color.rgb, CoC, centerPoint, cornerPoint, bilerpWeights, int2(-2,  0));
	BleedBackground(color.rgb, CoC, centerPoint, cornerPoint, bilerpWeights, int2( 0, -2));
	BleedBackground(color.rgb, CoC, centerPoint, cornerPoint, bilerpWeights, int2(+2,  0));
	BleedBackground(color.rgb, CoC, centerPoint, cornerPoint, bilerpWeights, int2( 0, +2));
	BleedBackground(color.rgb, CoC, centerPoint, cornerPoint, bilerpWeights, int2(-2, -2));
	BleedBackground(color.rgb, CoC, centerPoint, cornerPoint, bilerpWeights, int2(+2, -2));
	BleedBackground(color.rgb, CoC, centerPoint, cornerPoint, bilerpWeights, int2(+2, +2));
	BleedBackground(color.rgb, CoC, centerPoint, cornerPoint, bilerpWeights, int2(-2, +2));
#endif

	// store to halfres buffer for subsequent DOF splatting pass
	dst[coord] = float4(color.rgb, dilatedCoC);
	//blurredLayers[uint3(coord, DOF::FOREGROUND_NEAR_LAYER)] = float4(1, 0, 0, 1) * (dilatedCoC - abs(CoC)) * 5e-2f;
#else
	float4 color = float4(DownsampleColor(center), 1);

	// store to halfres buffer for subsequent DOF splatting pass
	dst[coord] = float4(color.rgb, CoC);
#endif

	/*
	write directly to DOF blur layer to avoid rasterizing pixel-sized sprites
	do not blur sharp pixels with small CoC, also prevent them from bleeding via downsample
	*/
	//[branch]
	//if (abs(CoC) < Bokeh::R && abs(CoC) > /*.25f*/rsqrt(4 * radians(180)))
	[branch]
	if (abs(dilatedCoC) + .5f <= Bokeh::R && (color.a = DOF::OpacityHalfres(abs(dilatedCoC), cameraSettings.aperture)))
	{
		//color.a = DOF::OpacityHalfres(CoC);
		color.rgb *= color.a;
		blurredLayers[uint3(coord, CoC > 0 ? DOF::BACKGROUND_NEAR_LAYER : DOF::FOREGROUND_FAR_LAYER)] = color;
	}

	// downsample to halfres with 13-tap partial Karis filter

	float4 block =
		src.SampleLevel(tapFilter, center, 0, int2(-1, -1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(+1, -1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(-1, +1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(+1, +1)) * .25f;

	color.rgb = DecodeHDRExp(block, .5f/*block weight*/ * cameraSettings.exposure);

	// shared taps
	const float4
		C = src.SampleLevel(tapFilter, center, 0) * .25f,
		W = src.SampleLevel(tapFilter, center, 0, int2(-2, 0)),
		E = src.SampleLevel(tapFilter, center, 0, int2(+2, 0)),
		N = src.SampleLevel(tapFilter, center, 0, int2(0, -2)),
		S = src.SampleLevel(tapFilter, center, 0, int2(0, +2));

	block = src.SampleLevel(tapFilter, center, 0, int2(-2, -2)) * .25f + C + W * .25f + N * .25f;
	color.rgb += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, center, 0, int2(+2, -2)) * .25f + C + E * .25f + N * .25f;
	color.rgb += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, center, 0, int2(-2, +2)) * .25f + C + W * .25f + S * .25f;
	color.rgb += DecodeHDRExp(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, center, 0, int2(+2, +2)) * .25f + C + E * .25f + S * .25f;
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