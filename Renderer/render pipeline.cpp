#include "stdafx.h"
#include "render pipeline.h"
#include "render stage.h"

using namespace std;
using namespace Renderer::Impl;
using namespace RenderPipeline;

static queue<future<PipelineStage>> pipeline;
static const IRenderStage *curRenderStage;

// returns std::monostate on stage waiting/pipeline finish, null RenderStageItem on batch overflow
PipelineItem RenderPipeline::GetNext(unsigned int &length)
{
	if (!curRenderStage)
	{
		if (!pipeline.empty() && pipeline.front().wait_for(0s) != future_status::timeout)
		{
			const auto stage = pipeline.front().get();
			pipeline.pop();

			// if pipeline stage is cmd list
			if (const auto cmdList = get_if<ID3D12GraphicsCommandList2 *>(&stage))
				return *cmdList;

			// else pipeline stage is render stage
			{
				const auto &renderStage = get<RenderStage>(stage);
				(curRenderStage = renderStage.first)->Sync(renderStage.second);
			}
		}
	}
	return curRenderStage ? curRenderStage->GetNextWorkItem(length) : PipelineItem{};
}

void RenderPipeline::TerminateStageTraverse() noexcept
{
	curRenderStage = nullptr;
}

void RenderPipeline::AppendStage(future<PipelineStage> &&stage)
{
	pipeline.push(move(stage));
}

bool RenderPipeline::Empty() noexcept
{
	return !curRenderStage && pipeline.empty();
}