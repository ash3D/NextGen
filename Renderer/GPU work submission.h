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
#if 0
			AppendRenderStage(packaged_task<RenderPipeline::PipelineStage()>(bind(forward<F>(f), forward<Args>(args)...)));
#elif defined _MSC_VER && _MSC_VER <= 1928
#if 1
			AppendRenderStage(packaged_task<RenderPipeline::PipelineStage()>(_Fake_no_copy_callable_adapter([task = std::async(launch::deferred, forward<F>(f), forward<Args>(args)...)]() mutable -> RenderPipeline::PipelineStage { return task.get(); })));
#else
			AppendRenderStage(packaged_task<RenderPipeline::PipelineStage()>([task = std::async(launch::deferred, forward<F>(f), forward<Args>(args)...).share()]() -> RenderPipeline::PipelineStage
			{
				return move(const_cast<remove_cvref_t<decltype(task.get())> &>(task.get()));
			}));
#endif
#else
			AppendRenderStage(packaged_task<RenderPipeline::PipelineStage()>([task = std::async(launch::deferred, forward<F>(f), forward<Args>(args)...)]() mutable -> RenderPipeline::PipelineStage { return task.get(); }));
#endif
		}
		else
			RenderPipeline::AppendStage(std::async(launch::deferred, forward<F>(f), forward<Args>(args)...));
	}

	void Run();
}