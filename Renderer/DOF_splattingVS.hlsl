#include "DOF.hlsli"
#include "Bokeh.hlsli"
#include "camera params.hlsli"

SamplerState COCdownsampleFilter : register(s0);
Texture2D src : register(t5);
Texture2D<float2> COCbuffer : register(t4);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

// scanline layout - same considerations as for lens flare applicable here
DOF::SplatPoint main(in uint flatPixelIdx : SV_VertexID)
{
	uint2 srcSize;
	src.GetDimensions(srcSize.x, srcSize.y);
	const uint2 coord = { flatPixelIdx % srcSize.x, flatPixelIdx / srcSize.x };

	float2 center = (coord + .5f) / srcSize;

	float2 fullres;
	COCbuffer.GetDimensions(fullres.x, fullres.y);
	const float2 centerPoint = floor(center * fullres + .5f) / fullres;
	const float CoC = DOF::SelectCoC(COCbuffer.Gather(COCdownsampleFilter, centerPoint));

	// transform 'center' UV -> NDC
	center *= 2;
	center -= 1;
	center.y = -center.y;

	const float4 fetch = src[coord];

	const DOF::SplatPoint splatPoint =
	{
		center,
		(abs(fetch.a) + .5f) * (2/*[0..1] -> NDC [-1..+1]*/ / Bokeh::R/*treat inner R as CoC, blow quad to fit*/) / srcSize,
		cameraSettings.apertureRot,
		CoC, fetch.a,
		cameraSettings.aperture,
		fetch.rgb
	};

	return splatPoint;
}