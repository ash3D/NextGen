#include "CS config.hlsli"
#include "camera params.hlsli"

Texture2DMS<float> ZBuffer : register(t0);
RWTexture2D<float2> COCbuffer : register(u4);
ConstantBuffer<CameraParams::Settings> cameraSettings : register(b1);

[numthreads(CSConfig::ImageProcessing::blockSize, CSConfig::ImageProcessing::blockSize, 1)]
void main(in uint2 coord : SV_DispatchThreadID)
{
	uint2 size;
	uint MSAA;
	ZBuffer.GetDimensions(size.x, size.y, MSAA);

	float2 CoC = cameraSettings.COC(ZBuffer[coord]);

	for (uint sampleIdx = 1; sampleIdx < MSAA; sampleIdx++)
	{
		const float CoCSample = cameraSettings.COC(ZBuffer.sample[sampleIdx][coord]);
		CoC.x = min(CoC.x, CoCSample);
		CoC.y += CoCSample;
	}

	CoC.y /= MSAA;

	COCbuffer[coord] = CoC;
}