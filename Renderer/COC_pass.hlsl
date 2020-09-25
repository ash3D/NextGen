#include "CS config.hlsli"
#include "DOF.hlsli"
#include "camera params.hlsli"

Texture2DMS<float> ZBuffer : register(t0);
RWTexture2D<float> COCbuffer : register(u5);
RWTexture2D<float> DOFopacityBuffer : register(u4);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	uint2 size;
	uint MSAA;
	ZBuffer.GetDimensions(size.x, size.y, MSAA);

	float CoC = cameraSettings.COC(ZBuffer[coord]);
	float opacity = DOF::OpacityFullres(CoC, cameraSettings.aperture);

	for (uint sampleIdx = 1; sampleIdx < MSAA; sampleIdx++)
	{
		const float CoCSample = cameraSettings.COC(ZBuffer.sample[sampleIdx][coord]);
		CoC = min(CoC, CoCSample);
		opacity += DOF::OpacityFullres(CoCSample, cameraSettings.aperture);
	}

	opacity /= MSAA;

	COCbuffer[coord] = CoC;
	DOFopacityBuffer[coord] = opacity;
}