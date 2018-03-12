/**
\author		Alexey Shaydurov aka ASH
\date		12.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

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