#pragma once

#include "stdafx.h"

namespace Renderer::Config
{
	constexpr DXGI_FORMAT HDRFormat = DXGI_FORMAT_R16G16B16A16_FLOAT, DisplayFormat = DXGI_FORMAT_R10G10B10A2_UNORM, ZFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	namespace Aniso
	{
		namespace Terrain
		{
			constexpr UINT albedo = 16, fresnel = 12, roughness = 4, bump = 8;
		}
	}
	DXGI_SAMPLE_DESC MSAA();
}