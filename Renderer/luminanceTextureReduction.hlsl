#include "CS config.hlsli"
#include "HDR codec.hlsli"
#include "luminance.hlsli"

namespace Tonemapping
{
	static const uint localDataSize = CSConfig::LuminanceReduction::TexturePass::groupSize * CSConfig::LuminanceReduction::TexturePass::groupSize;
}
#include "tonemapLocalReduction.hlsli"

#define DXC_NAMESPACE_WORKAROUND 1

#if DXC_NAMESPACE_WORKAROUND
Texture2D src : register(t0);
RWByteAddressBuffer dst : register(u1);
#endif

namespace Tonemapping
{
	Texture2D src : register(t0);
	RWByteAddressBuffer dst : register(u1);
}

// !: no low-level optimizations yet (e.g. GPR pressure)
[numthreads(CSConfig::LuminanceReduction::TexturePass::groupSize, CSConfig::LuminanceReduction::TexturePass::groupSize, 1)]
void main(in uint2 globalIdx : SV_DispatchThreadID, in uint flatLocalIdx : SV_GroupIndex, in uint2 groupIdx : SV_GroupID)
{
	//using namespace Tonemapping;

	uint2 srcSize;
	src.GetDimensions(srcSize.x, srcSize.y);

	float2 partialReduction = 0;
	const float weight = rcp(srcSize.x * srcSize.y);

	const uint2 dispatchSize = CSConfig::LuminanceReduction::TexturePass::DispatchSize(srcSize), interleaveStride = dispatchSize * CSConfig::LuminanceReduction::TexturePass::blockSize;

	// interleaved tile reduction
	for (uint R = globalIdx.y * CSConfig::LuminanceReduction::TexturePass::tileSize; R < srcSize.y; R += interleaveStride.y)
		for (uint C = globalIdx.x * CSConfig::LuminanceReduction::TexturePass::tileSize; C < srcSize.x; C += interleaveStride.x)
			[unroll]
			for (uint r = 0; r < CSConfig::LuminanceReduction::TexturePass::tileSize; r++)
				[unroll]
				for (uint c = 0; c < CSConfig::LuminanceReduction::TexturePass::tileSize; c++)
				{
					const float3 srcPixel = DecodeHDR(src.Load(uint3(C, R, 0), uint2(c, r)));
					/*
						'max' used to convert NaN to 0
						NaN comes from out-of-bounds pixels in edge tiles - they are which fetched as 0
						and 0/0 produces NaN according to DirectX floating-point rules (https://docs.microsoft.com/en-us/windows/desktop/direct3d11/floating-point-rules)
						'min' clamps fp16 overflow (inf)
						can't use clamp here as it doesn't handle NaN
					*/
					const float lum = min(max(RGB_2_luminance(srcPixel), 0), 64e3f);
					// do weighting per-pixel rather than once in final reduction since mul is free here (merged into mad)\
					NOTE: first iteration for unrolled loop can optimize add out thus making mul non-free
					partialReduction.x += log2(lum + 1) * weight;
					partialReduction.y = max(partialReduction.y, lum);
				}

	// bulk of reduction work
	const float2 finalReduction = Tonemapping::LocalReduce(partialReduction, flatLocalIdx);

	// store result to global buffer
	const uint flatGroupIdx = groupIdx.y * dispatchSize.x + groupIdx.x;
	if (flatLocalIdx == 0)
		dst.Store2(flatGroupIdx * 8, asuint(finalReduction));
}