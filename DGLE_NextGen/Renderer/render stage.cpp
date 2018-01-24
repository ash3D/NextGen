/**
\author		Alexey Shaydurov aka ASH
\date		24.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "render stage.h"

using namespace std;
using namespace Renderer::Impl::RenderPipeline;

RenderStageItem IRenderStage::IterateRenderPass(unsigned int &length, const signed long int passLength,
	const function<void ()> &NextStageItem, const function<RenderStageItem (unsigned long rangeBegin, unsigned long rangeEnd)> &GetRenderRange) const
{
	// render pipeline traversed sequentially in main thread => can use 'static'
	static unsigned long curRangeBegin;
	assert(curRangeBegin <= passLength);
	__assume(curRangeBegin <= passLength);

	// next render item if current pass is exhausted
	if (curRangeBegin == passLength)
	{
		curRangeBegin = 0;
		NextStageItem();
		return GetNextRenderItem(length);
	}

	if (length)
	{
		signed long curRangeEnd = curRangeBegin + length;
		if (const signed long tail = curRangeEnd - passLength; tail > 0)
		{
			curRangeEnd = passLength;
			length = tail;
		}
		else
			length = 0;

		// not const to allow move on return
		auto result = GetRenderRange(curRangeBegin, curRangeEnd);
		curRangeBegin = curRangeEnd;
		return move(result);
	}

	return nullptr;
}