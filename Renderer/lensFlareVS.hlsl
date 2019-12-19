#include "lensFlare.hlsli"
#include "camera params.hlsli"
#include "HDR codec.hlsli"
#include "luminance.hlsli"

SamplerState tapFilter : register(s0);
Texture2D src : register(t0);
RWTexture2D<float4> dst : register(u0, space1);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b0);

/*
	scanline layout - probably not cache efficient
	tiled swizzling pattern can be faster - experiments on different GPUs wanted
*/
LensFlare::Source main(in uint flatPixxelIdx : SV_VertexID)
{
	uint2 dstSize;
	dst.GetDimensions(dstSize.x, dstSize.y);
	const uint2 coord = { flatPixxelIdx % dstSize.x, flatPixxelIdx / dstSize.x };
	float2 center = (coord + .5f) / dstSize;

	// downsample to halfres with 13-tap partial Karis filter

	float4 block =
		src.SampleLevel(tapFilter, center, 0, int2(-1, -1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(+1, -1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(-1, +1)) * .25f +
		src.SampleLevel(tapFilter, center, 0, int2(+1, +1)) * .25f;

	float3 acc = DecodeHDR(block, .5f/*block weight*/ * cameraSettings.exposure);

	// shared taps
	const float4
		C = src.SampleLevel(tapFilter, center, 0) * .25f,
		W = src.SampleLevel(tapFilter, center, 0, int2(-2, 0)),
		E = src.SampleLevel(tapFilter, center, 0, int2(+2, 0)),
		N = src.SampleLevel(tapFilter, center, 0, int2(0, -2)),
		S = src.SampleLevel(tapFilter, center, 0, int2(0, +2));

	block = src.SampleLevel(tapFilter, center, 0, int2(-2, -2)) * .25f + C + W * .25f + N * .25f;
	acc += DecodeHDR(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, center, 0, int2(+2, -2)) * .25f + C + E * .25f + N * .25f;
	acc += DecodeHDR(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, center, 0, int2(-2, +2)) * .25f + C + W * .25f + S * .25f;
	acc += DecodeHDR(block, .125f/*block weight*/ * cameraSettings.exposure);

	block = src.SampleLevel(tapFilter, center, 0, int2(+2, +2)) * .25f + C + E * .25f + S * .25f;
	acc += DecodeHDR(block, .125f/*block weight*/ * cameraSettings.exposure);

	// store to halfres buffer for subsequent bloom passes
	dst[coord] = float4(acc, 1);

	// transform 'center' UV -> NDC
	center *= 2;
	center -= 1;
	center.y = -center.y;

	LensFlare::Source flareSource =
	{
		center,
		cameraSettings.aperture.xx,
		center * cameraSettings.aperture, length(center),
		acc, 0
	};
	flareSource.ext.x *= float(dstSize.y) / float(dstSize.x);

	// cull faint flares (leave 0 for dull pixels)
	if (RGB_2_luminance(acc) >= LensFlare::threshold)
	{
		// vignette
		flareSource.col.a = saturate(1.8f - dot(flareSource.pos, flareSource.pos));
		flareSource.col.a *= flareSource.col.a;

		// normalize
		flareSource.col.a *= LensFlare::normRebalance / (dstSize.x * dstSize.y);
	}

	return flareSource;
}