/**
\author		Alexey Shaydurov aka ASH
\date		28.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"

// research needed: consider smoothing blending of various params with different weights, take screen resoulution into account etc
namespace Renderer::Impl::OcclusionCulling
{
	// C++17
	/*inline*/ constexpr float nodeProjLengthThreshold = 5e-2f, nestedNodeProjLengthShrinkThreshold = .4f, parentOcclusionThreshold = .8f, accumulatedChildrenMeasureShrinkThreshold = .7f;
	/*inline*/ constexpr unsigned maxOcclusionQueryBoxes = 64;
	/*inline*/ constexpr unsigned long int exclusiveTriCountCullThreshold = 256;

	inline bool QueryBenefit(float aabbSquare, unsigned long int triCount)
	{
		// need research
		constexpr unsigned int minTriCount = 4096u;
		constexpr float threshold = minTriCount / .2f;

		if (triCount < minTriCount)
			return false;

		return triCount / sqrt(aabbSquare) >= threshold;
	}
}