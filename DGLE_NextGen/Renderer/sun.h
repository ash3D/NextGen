/**
\author		Alexey Shaydurov aka ASH
\date		25.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"

namespace Renderer::Sun
{
	namespace HLSL = Math::VectorMath::HLSL;

	HLSL::float3 Dir(float zenith, float azimuth);
	HLSL::float3 Irradiance(float zenith, float cosZenith);
}