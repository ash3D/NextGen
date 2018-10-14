/**
\author		Alexey Shaydurov aka ASH
\date		15.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#ifndef REDUCTION_TXTURE_CONFIG_INCLUDED
#define REDUCTION_TXTURE_CONFIG_INCLUDED

static const unsigned int groupSize = 16, tileSize = 2, blockSize = groupSize * tileSize; // ^2
static const uint2 maxDispatchSize = {64, 32};	// output buffer max 2048 for final reduce

inline uint2 DispatchSize(uint2 srcSize)
{
	const uint2
		blockCount = (srcSize + (blockSize - 1)) / blockSize,
		interleaveFactor = (blockCount + maxDispatchSize - 1) / maxDispatchSize,
		interleavedSize = blockSize * interleaveFactor;
	return (srcSize + interleavedSize - 1) / interleavedSize;
}

#endif	// REDUCTION_TXTURE_CONFIG_INCLUDED