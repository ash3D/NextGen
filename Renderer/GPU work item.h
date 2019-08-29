#pragma once

#include <utility>	// for forward
#include <functional>
#include <variant>

struct ID3D12GraphicsCommandList4;

namespace Renderer::Impl::CmdListPool
{
	class CmdList;
}

namespace Renderer::Impl::RenderPipeline
{
	struct RenderStageItem
	{
		typedef std::function<void (CmdListPool::CmdList &)> Work;
		Work work;
		bool suspended;

	public:
		// TODO: use C++20 auto instead of template
		template<typename F>
		explicit RenderStageItem(F &&f, bool suspended = false) : work(std::forward<F>(f)), suspended(suspended) {}
	};

	struct PipelineItem : std::variant<std::monostate, ID3D12GraphicsCommandList4 *, RenderStageItem>
	{
		template<typename ...RenderStageArgs>
		explicit PipelineItem(RenderStageArgs &&...args) : variant(std::in_place_type<RenderStageItem>, std::forward<RenderStageArgs>(args)...) {}

	private:
		friend PipelineItem GetNext(unsigned int &length);
		PipelineItem() = default;
		PipelineItem(ID3D12GraphicsCommandList4 *cmdList) : variant(cmdList) {}
	};
}