/**
\author		Alexey Shaydurov aka ASH
\date		11.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "render pipeline.h"
#include "cmdlist pool.h"

using namespace std;
using namespace Renderer;
using namespace Impl;
using namespace RenderPipeline;

static queue<future<PipelineStage>> pipeline;
static unsigned long int curRenderRangeBegin;
static const IRenderStage *curRenderStage;
static unsigned curRenderPassIdx;

// 1 call site
static inline void NextRenderPass()
{
	assert(curRenderStage);
	curRenderRangeBegin = 0;
	if (++curRenderPassIdx >= curRenderStage->RenderPassCount())
	{
		curRenderStage = nullptr;
		curRenderPassIdx = 0;
	}
}

// 1 call site
static inline auto GetNextRenderRange(unsigned int &length) -> decltype(GetNext(length))
{
	assert(curRenderStage);
	if (length)
	{
		signed long int curRenderRangeEnd = curRenderRangeBegin + length;
		const signed long int passLength = (*curRenderStage)[curRenderPassIdx].Length();
		if (const signed long int tail = curRenderRangeEnd - passLength; tail > 0)
		{
			curRenderRangeEnd = passLength;
			length = tail;
		}
		else
			length = 0;

		// not const to allow move on return
		decltype(GetNext(length)) result(in_place_type<RenderRange>, curRenderRangeBegin, curRenderRangeEnd, curRenderStage, curRenderPassIdx);
		curRenderRangeBegin = curRenderRangeEnd;

		// next render pass if current one is exhausted
		if (curRenderRangeEnd == passLength)
			NextRenderPass();

		return result;
	}
	else
		return {};
}

inline RenderRange::RenderRange(unsigned long int rangeBegin, unsigned long int rangeEnd, const IRenderStage *renderStage, unsigned renderPassIdx) noexcept :
rangeBegin(rangeBegin), rangeEnd(rangeEnd), renderStage(renderStage), renderPassIdx(renderPassIdx)
{}

void RenderRange::operator ()(CmdListPool::CmdList &target) const
{
	assert(renderStage);
	const IRenderPass &renderPass = (*renderStage)[renderPassIdx];
	target.Setup(renderPass.GetPSO(rangeBegin));
	renderPass(*renderStage, rangeBegin, rangeEnd, target);
}

auto RenderPipeline::GetNext(unsigned int &length) -> decltype(GetNext(length))
{
	if (!curRenderStage)
	{
		if (!pipeline.empty() && pipeline.front().wait_for(0s) != future_status::timeout)
		{
			const auto stage = pipeline.front().get();
			pipeline.pop();

			// if pipelone stage is cmd list
			if (const auto cmdList = get_if<ID3D12GraphicsCommandList1 *>(&stage))
				return *cmdList;

			// else pipeline stage is render stage
			curRenderStage = get<const IRenderStage *>(stage);
			curRenderStage->Sync();
		}
	}
	return curRenderStage ? GetNextRenderRange(length) : decltype(GetNext(length)){};
}

void RenderPipeline::AppendStage(future<PipelineStage> &&stage)
{
	pipeline.push(move(stage));
}

bool RenderPipeline::Empty()
{
	return !curRenderStage && pipeline.empty();
}