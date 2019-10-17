#pragma once

namespace Materials
{
	cbuffer MaterialConstants : register(b1, space1)
	{
		float3	albedo;
		float	TVBrighntess;
	}
}