#pragma once

#include "stdafx.h"

namespace Renderer::Config
{
	constexpr DXGI_FORMAT
		HDRFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
		DisplayFormat = DXGI_FORMAT_R10G10B10A2_UNORM,
		DOFLayersFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;	// very expensive, try dithering / sprite batching to enable fp16 without banding / tint distortion artifacts
	namespace ZFormat
	{
		constexpr DXGI_FORMAT ROP = DXGI_FORMAT_D24_UNORM_S8_UINT, shader = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	}
	constexpr FLOAT TVBacklightBleed = .2F;
	namespace Aniso
	{
		namespace Terrain
		{
			constexpr UINT albedo = 14, fresnel = 12, roughness = 5, bump = 7;
		}

		namespace Object3D
		{
			constexpr UINT albedo = 16, bump = 8, glassMask = 4, TV = 16;
		}
	}
	DXGI_SAMPLE_DESC MSAA();
}