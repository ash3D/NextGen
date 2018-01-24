/**
\author		Alexey Shaydurov aka ASH
\date		24.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "render pipeline.h"
#include "render stage.h"

using namespace std;
using namespace Renderer::Impl;
using namespace RenderPipeline;

static queue<future<PipelineStage>> pipeline;
static const IRenderStage *curRenderStage;

// returns std::monostate on stage waiting/pipeline finish, null RenderStageItem on batch oferflow
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
	return curRenderStage ? curRenderStage->GetNextRenderItem(length) : decltype(GetNext(length)){};
}

void RenderPipeline::TerminateStageTraverse() noexcept
{
	curRenderStage = nullptr;
}

void RenderPipeline::AppendStage(future<PipelineStage> &&stage)
{
	pipeline.push(move(stage));
}

bool RenderPipeline::Empty()
{
	return !curRenderStage && pipeline.empty();
}