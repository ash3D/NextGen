#pragma once

namespace Tonemapping
{
	static const unsigned int blockSize = 8;

	namespace BufferReduction
	{
		static const unsigned int blockSize = 1024;
	}

	namespace TextureReduction
	{
		static const unsigned int groupSize = 16, tileSize = 2, blockSize = groupSize * tileSize; // ^2
#if 0
		// DXC crash
		static const uint2 maxDispatchSize = { 64, 32 };	// output buffer max 2048 for final reduce
#endif

		inline uint2 DispatchSize(uint2 srcSize)
		{
			static const uint2 maxDispatchSize = { 64, 32 };	// output buffer max 2048 for final reduce
			const uint2
				blockCount = (srcSize + (blockSize - 1)) / blockSize,
				interleaveFactor = (blockCount + maxDispatchSize - 1) / maxDispatchSize,
				interleavedSize = blockSize * interleaveFactor;
			return (srcSize + interleavedSize - 1) / interleavedSize;
		}
	}
}