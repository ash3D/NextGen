#pragma once

#include "stdafx.h"

namespace Renderer::Sun
{
	namespace HLSL = Math::VectorMath::HLSL;

	HLSL::float3 Dir(float zenith, float azimuth);
	HLSL::float3 Irradiance(float zenith, float cosZenith);
}