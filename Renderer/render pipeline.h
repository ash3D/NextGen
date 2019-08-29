#pragma once

#include <memory>
#include <variant>
#include <future>
#include "GPU work item.h"

struct ID3D12GraphicsCommandList4;

namespace Renderer::Impl::RenderPipeline
{
	typedef std::shared_ptr<const class IRenderStage> RenderStage;
	typedef std::variant<ID3D12GraphicsCommandList4 *, RenderStage> PipelineStage;

	void AppendStage(std::future<PipelineStage> &&stage);
	PipelineItem GetNext(unsigned int &length);
	void TerminateStageTraverse() noexcept;
	bool Empty() noexcept;
}