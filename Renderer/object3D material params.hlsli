#pragma once

namespace Materials
{
	struct Common
	{
		float	roughness, f0;
	};

	struct Flat : Common
	{
		float3	albedo;
	};

	struct TV : Common
	{
		float	TVBrighntess;
		float3	albedo;
	};
}