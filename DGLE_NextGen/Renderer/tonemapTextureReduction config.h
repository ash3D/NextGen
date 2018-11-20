/**
\author		Alexey Shaydurov aka ASH
\date		17.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"

namespace Renderer::ReductionTextureConfig
{
	using Math::VectorMath::HLSL::uint2;

#	include "tonemapTextureReduction config.hlsli"
}