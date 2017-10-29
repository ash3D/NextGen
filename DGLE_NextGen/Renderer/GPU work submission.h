/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "../render pipeline.h"

namespace Renderer::GPUWorkSubmission
{
	namespace RenderPipeline = Impl::RenderPipeline;

	void Prepare();

	template<typename F, typename ...Args>
	void AppendCmdList(F &&f, Args &&...args)
	{
		using namespace std;
		RenderPipeline::AppendStage(async(launch::deferred, forward<F>(f), forward<Args>(args)...));
	}

	template<typename F, typename ...Args>
	void AppendRenderStage(F &&f, Args &&...args)
	{
		using namespace std;
		extern void LaunchBuildRenderStage(packaged_task<RenderPipeline::PipelineStage ()> &&buildRenderStage);
		extern vector<future<void>> pendingAsyncRefs;
		packaged_task<RenderPipeline::PipelineStage ()> buildRenderStage(bind(forward<F>(f), forward<Args>(args)...));
		RenderPipeline::AppendStage(buildRenderStage.get_future());
		pendingAsyncRefs.push_back(async(launch::async, LaunchBuildRenderStage, move(buildRenderStage)));
	}

	void Run();
}