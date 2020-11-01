#pragma once

namespace CSConfig
{
	namespace ImageProcessing
	{
		static const unsigned int blockSize = 8;
	}

	namespace LuminanceReduction
	{
		namespace BufferPass
		{
			static const unsigned int blockSize = 1024;
		}

		namespace TexturePass
		{
			static const unsigned int groupSize = 16, tileSize = 2, blockSize = groupSize * tileSize; // ^2
			static const uint2 maxDispatchSize = { 64, 32 };	// output buffer max 2048 for final reduce

			inline uint2 DispatchSize(uint2 srcSize)
			{
				const uint2
					blockCount = (srcSize + (blockSize - 1)) / blockSize,
					interleaveFactor = (blockCount + maxDispatchSize - 1) / maxDispatchSize,
					interleavedSize = blockSize * interleaveFactor;
				return (srcSize + interleavedSize - 1) / interleavedSize;
			}
		}
	}
}