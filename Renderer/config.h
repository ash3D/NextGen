/**
\author		Alexey Shaydurov aka ASH
\date		22.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"

namespace Renderer::Config
{
	constexpr DXGI_FORMAT HDRFormat = DXGI_FORMAT_R16G16B16A16_FLOAT, DisplayFormat = DXGI_FORMAT_R10G10B10A2_UNORM, ZFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGI_SAMPLE_DESC MSAA();
}