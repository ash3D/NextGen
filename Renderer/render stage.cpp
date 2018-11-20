#include "stdafx.h"
#include "render stage.h"
#include "render pipeline.h"

using namespace std;
using namespace Renderer::Impl::RenderPipeline;

// static definition
PipelineItem (IRenderStage::*IRenderStage::phaseSelector)(unsigned int &length) const;

PipelineItem IRenderStage::IterateRenderPass(unsigned int &length, const signed long int passLength,
	const function<void ()> &PassFinish, const function<RenderStageItem (unsigned long rangeBegin, unsigned long rangeEnd)> &GetRenderRange) const
{
	// render pipeline traversed sequentially in main thread => can use 'static'
	static unsigned long curRangeBegin;
	assert(curRangeBegin <= passLength);
	__assume(curRangeBegin <= passLength);

	// next pipeline item if current pass is exhausted
	if (curRangeBegin == passLength)
	{
		curRangeBegin = 0;
		PassFinish();
		return GetNext(length);
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

	return PipelineItem{ in_place_type<RenderStageItem> };
}