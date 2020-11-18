#include "stdafx.h"
#include "render stage.h"
#include "render passes.h"
#include "render pipeline.h"
#include "cmdlist pool.inl"

using namespace std;
using namespace Renderer::Impl;
using namespace RenderPipeline;
using namespace RenderPasses;

// static definitions
PipelineItem (IRenderStage::*IRenderStage::phaseSelector)(unsigned int &length) const;
unsigned long IRenderStage::curRangeBegin;

static void FastForward(CmdListPool::CmdList &cmdList, const optional<PassROPBinding<RenderStageRTBinding>> &RTBinding, const PassROPBinding<RenderStageZBinding> &ZBinding)
{
	cmdList.Setup(NULL);

	if (RTBinding)
		RTBinding->stageBinding.FastForward(cmdList, RTBinding->open, RTBinding->close);
	ZBinding.stageBinding.FastForward(cmdList, ZBinding.open, ZBinding.close);
}

PipelineItem IRenderStage::IterateRenderPass(unsigned int &length, const signed long int passLength,
	const function<void ()> &PassFinish, const function<RenderStageItem::Work (unsigned long rangeBegin, unsigned long rangeEnd)> &GetRenderRange) const
{
	return IterateRenderPass(length, passLength, PassFinish, bind_front(GetNext, ref(length)), bind_front(GetRenderRange, curRangeBegin));
}

PipelineItem IRenderStage::IterateRenderPass(unsigned int &length, const signed long int passLength,
	const PassROPBinding<RenderStageRTBinding> *RTBinding, const PassROPBinding<RenderStageZBinding> &ZBinding, const ROPOutput &output,
	const function<void ()> &PassFinish, const function<RenderStageItem::Work (unsigned long rangeBegin, unsigned long rangeEnd, const RangeRenderPass &renderPass)> &GetRenderRange) const
{
	const auto PassExhausted = [&]
	{
		using namespace placeholders;
		return passLength ? GetNext(length) : PipelineItem{ bind(FastForward, _1, RTBinding ? optional(*RTBinding) : nullopt, ZBinding) };
	};
	const auto GetRenderRangeWrapper = [&](signed long curRangeEnd)
	{
		const RangeRenderPass renderPass(RTBinding, ZBinding, output, curRangeBegin == 0, curRangeEnd == passLength);
		return PipelineItem{ GetRenderRange(curRangeBegin, curRangeEnd, renderPass), renderPass.Suspended() };
	};
	return IterateRenderPass(length, passLength, PassFinish, PassExhausted, GetRenderRangeWrapper);
}

template<typename PassExhaustedCallback, typename GetRenderRangeCallback>
inline PipelineItem IRenderStage::IterateRenderPass(unsigned int &length, const signed long int passLength,
	const function<void ()> &PassFinish, const PassExhaustedCallback &PassExhausted, const GetRenderRangeCallback &GetRenderRange) const
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
		PipelineItem result{ GetRenderRange(curRangeEnd) };
		curRangeBegin = curRangeEnd;
		return result;
	}

	return PipelineItem{ nullptr };
}