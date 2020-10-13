#include "stdafx.h"
#include "config.h"

using Microsoft::WRL::ComPtr;

extern ComPtr<ID3D12Device4> device;

DXGI_SAMPLE_DESC Renderer::Config::MSAA()
{
	// query for color
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS MSAA_info = { HDRFormat, 8/*MSAA 8x support required on 11.0 hardware*/, D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE };
	CheckHR(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &MSAA_info, sizeof MSAA_info));
	DXGI_SAMPLE_DESC result = { MSAA_info.SampleCount, MSAA_info.NumQualityLevels };

	// query for Z, select min
	MSAA_info.Format = ZFormat::ROP;
	CheckHR(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &MSAA_info, sizeof MSAA_info));
	result.Quality = std::min(result.Quality, MSAA_info.NumQualityLevels) - 1u;

	return result;
}