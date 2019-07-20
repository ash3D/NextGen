#pragma once

// HLSL doesn't support C++17 stile nested namespaces yet
#ifdef __cplusplus
namespace Renderer::Impl::Descriptors::TextureSamplers
#else
namespace TextureSamplers
#endif
{
	enum Object3DCommonSamplerID
	{
		OBJ3D_ALBEDO_SAMPLER,
		OBJ3D_ALBEDO_ALPHATEST_SAMPLER,
		OBJ3D_BUMP_SAMPLER,
		OBJ3D_GLASS_MASK_SAMPLER,
		OBJ3D_COMMON_SAMPLERS_COUNT
	};
}