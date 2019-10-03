#pragma once

#define ENABBLE_TEX

namespace Materials
{
	enum TextureID
	{
		ALBEDO_MAP,
		NORMAL_MAP,
		GLASS_MASK,
	};
}

#include "object3D generic material.hlsli"