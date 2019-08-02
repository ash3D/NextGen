#pragma once

#include <functional>
#include <variant>

struct ID3D12GraphicsCommandList4;

namespace Renderer::CmdListPool
{
	class CmdList;
}

namespace Renderer::Impl::RenderPipeline
{
	struct RenderStageItem
	{
		typedef std::function<void (CmdListPool::CmdList &)> Work;
		Work work;
		bool suspended = false;
	};
	typedef std::variant<std::monostate, ID3D12GraphicsCommandList4 *, RenderStageItem> PipelineItem;
}