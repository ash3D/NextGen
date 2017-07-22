/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

namespace Renderer::Impl::OcclusionCulling
{
	// C++17
	/*inline*/ constexpr float nestedNodeSquareThreshold = .1f, parentOcclusionThreshold = .8f, accumulatedChildrenMeasureThreshold = .8f;
	/*inline*/ constexpr unsigned maxOcclusionQueryBoxes = 64;

	inline bool QueryBenefit(float aabbSquare, unsigned long int triCount)
	{
		// need research
		constexpr unsigned int minTriCount = 4096u;
		constexpr float threshold = minTriCount / 16e-4f;

		if (triCount < minTriCount)
			return false;

		return triCount / (aabbSquare * aabbSquare) >= threshold;
	}
}