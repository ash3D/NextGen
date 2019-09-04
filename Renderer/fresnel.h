#pragma once

#include "general math.h"	// for lerp

namespace Renderer
{
	namespace Fresnel
	{
		using Math::lerp;
	}

#	include "fresnel.hlsli"
}