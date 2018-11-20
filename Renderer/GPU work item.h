#pragma once

#include <functional>
#include <variant>

struct ID3D12GraphicsCommandList2;

namespace Renderer::CmdListPool
{
	class CmdList;
}

namespace Renderer::Impl::RenderPipeline
{
	typedef std::function<void (CmdListPool::CmdList &)> RenderStageItem;
	typedef std::variant<std::monostate, ID3D12GraphicsCommandList2 *, RenderStageItem> PipelineItem;
}