#pragma once

#include "stdafx.h"
#include "render pipeline.h"

namespace Renderer::GPUWorkSubmission
{
	namespace RenderPipeline = Impl::RenderPipeline;

	void Prepare();

	template<bool async, typename F, typename ...Args>
	void AppendPipelineStage(F &&f, Args &&...args)
	{
		using namespace std;
		if constexpr (async)
		{
			extern void AppendRenderStage(packaged_task<RenderPipeline::PipelineStage()> &&buildRenderStage);
			AppendRenderStage(packaged_task<RenderPipeline::PipelineStage()>(bind(forward<F>(f), forward<Args>(args)...)));
		}
		else
			RenderPipeline::AppendStage(std::async(launch::deferred, forward<F>(f), forward<Args>(args)...));
	}

	void Run();
}