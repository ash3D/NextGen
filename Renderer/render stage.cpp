#include "stdafx.h"
#include "render stage.h"
#include "render pipeline.h"
#include "cmdlist pool.inl"

using namespace std;
using namespace Renderer::Impl;
using namespace RenderPipeline;
using namespace RenderPasses;

// static definitions
PipelineItem (IRenderStage::*IRenderStage::phaseSelector)(unsigned int &length) const;
unsigned long IRenderStage::curRangeBegin;

static void FastForward(CmdListPool::CmdList &cmdList, const optional<PassROPBinding<StageRTBinding>> &RTBinding, const PassROPBinding<StageZBinding> &ZBinding)
{
	cmdList.Setup(NULL);

	if (RTBinding)
		RTBinding->stageBinding.FastForward(cmdList, RTBinding->open, RTBinding->close);
	ZBinding.stageBinding.FastForward(cmdList, ZBinding.open, ZBinding.close);
}

PipelineItem IRenderStage::IterateRenderPass(unsigned int &length, const signed long int passLength,
	const function<void ()> &PassFinish, const function<RenderStageItem::Work (unsigned long rangeBegin, unsigned long rangeEnd)> &GetRenderRange) const
{
	using namespace placeholders;
	return IterateRenderPass(length, passLength, PassFinish, bind(GetNext, ref(length)), bind(GetRenderRange, curRangeBegin, _1));
}

PipelineItem IRenderStage::IterateRenderPass(unsigned int &length, const signed long int passLength,
	const PassROPBinding<StageRTBinding> *RTBinding, const PassROPBinding<StageZBinding> &ZBinding, const StageOutput &output,
	const function<void ()> &PassFinish, const function<RenderStageItem::Work (unsigned long rangeBegin, unsigned long rangeEnd, RenderPass &&renderPass)> &GetRenderRange) const
{
	const auto PassExhausted = [&]
	{
		using namespace placeholders;
		return passLength ? GetNext(length) : RenderStageItem{ bind(FastForward, _1, RTBinding ? optional(*RTBinding) : nullopt, ZBinding) };
	};
	const auto GetRenderRangeWrapper = [&](signed long curRangeEnd) -> RenderStageItem
	{
		RenderPass renderPass(RTBinding, ZBinding, output, curRangeBegin == 0, curRangeEnd == passLength);
		const bool suspended = renderPass.Suspended();
		return { GetRenderRange(curRangeBegin, curRangeEnd, move(renderPass)), suspended };
	};
	return IterateRenderPass(length, passLength, PassFinish, PassExhausted, GetRenderRangeWrapper);
}

template<typename PassExhausted, typename GetRenderRange>
inline PipelineItem IRenderStage::IterateRenderPass(unsigned int &length, const signed long int passLength,
	const std::function<void ()> &PassFinish, const PassExhausted &PassExhausted, const GetRenderRange &GetRenderRange) const
{
	assert(curRangeBegin <= passLength);
	__assume(curRangeBegin <= passLength);

	// next pipeline item if current pass is exhausted
	if (curRangeBegin == passLength)
	{
		curRangeBegin = 0;
		PassFinish();
		return PassExhausted();
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
		RenderStageItem result{ GetRenderRange(curRangeEnd) };
		curRangeBegin = curRangeEnd;
		return move(result);
	}

	return PipelineItem{ in_place_type<RenderStageItem> };
}