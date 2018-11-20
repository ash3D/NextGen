#pragma once

#include "stdafx.h"

// research needed: consider smoothing blending of various params with different weights, take screen resoulution into account etc
namespace Renderer::Impl::OcclusionCulling
{
	// C++17
	/*inline*/ constexpr float nodeProjLengthThreshold = 5e-2f, nestedNodeProjLengthShrinkThreshold = .4f, parentOcclusionThreshold = .8f, accumulatedChildrenMeasureShrinkThreshold = .7f;
	/*inline*/ constexpr unsigned maxOcclusionQueryBoxes = 64;
	/*inline*/ constexpr unsigned long int exclusiveTriCountCullThreshold = 256;
	/*inline*/ constexpr unsigned int minTriCount = 4096u;

	inline bool EarlyOut(unsigned long int triCount) noexcept
	{
		return triCount < minTriCount;
	}

	template<bool checkEarlyOut>
	inline bool QueryBenefit(float aabbSquare, unsigned long int triCount)
	{
		// need research
		constexpr float threshold = minTriCount / .2f;

		return triCount / sqrt(aabbSquare) >= threshold;
	}

	template<>
	inline bool QueryBenefit<true>(float aabbSquare, unsigned long int triCount)
	{
		return !EarlyOut(triCount) && QueryBenefit<false>(aabbSquare, triCount);
	}
}