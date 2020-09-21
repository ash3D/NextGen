#include "DOF.hlsli"
#include "Bokeh.hlsli"
#include "camera params.hlsli"

SamplerState COCdownsampler : register(s2);
Texture2D src : register(t6);
Texture2D<float2> fullresCOCbuffer : register(t4);
Texture2D<float1> dilatedCOCbuffer : register(t5);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

// scanline layout - same considerations as for lens flare applicable here
DOF::SplatPoint main(in uint flatPixelIdx : SV_VertexID)
{
	uint2 srcSize;
	src.GetDimensions(srcSize.x, srcSize.y);
	const uint2 coord = { flatPixelIdx % srcSize.x, flatPixelIdx / srcSize.x };
	float2 center = coord + .5f;

	float2 fullres;
	fullresCOCbuffer.GetDimensions(fullres.x, fullres.y);
	const float2 centerPoint = center * 2 / fullres;
	center /= srcSize;

	// transform 'center' UV -> NDC
	center *= 2;
	center -= 1;
	center.y = -center.y;

	const float
		CoC = DOF::DownsampleCoC(fullresCOCbuffer, COCdownsampler, centerPoint),
		dilatedCoC = dilatedCOCbuffer[coord];

	const DOF::SplatPoint splatPoint =
	{
		center,
		(dilatedCoC + .5f) * (2/*[0..1] -> NDC [-1..+1]*/ / Bokeh::R/*treat inner R as CoC, blow quad to fit*/) / srcSize,
		cameraSettings.apertureRot,
		CoC, dilatedCoC,
		cameraSettings.aperture,
		src[coord]
	};

	return splatPoint;
}