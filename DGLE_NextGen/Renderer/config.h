/**
\author		Alexey Shaydurov aka ASH
\date		08.05.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"

namespace Renderer::Config
{
	constexpr DXGI_FORMAT ColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM, ZFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	constexpr DXGI_SAMPLE_DESC MSAA() { return { 8, 0 }; }	// MSAA 8x support required on 11.0 hardware
}