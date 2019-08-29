#pragma once

#include "stdafx.h"
#include "GPU work item.h"
#include "render passes.h"

namespace Renderer::Impl::RenderPipeline
{
	class IRenderStage
	{
		virtual PipelineItem (IRenderStage::*DoSync(void) const)(unsigned int &length) const = 0;

	protected:
		static PipelineItem (IRenderStage::*phaseSelector)(unsigned int &length) const;

	private:
		// render pipeline traversed sequentially in main thread => can use 'static'
		static unsigned long curRangeBegin;

	protected:
		IRenderStage() = default;
		~IRenderStage() = default;
		IRenderStage(IRenderStage &) = delete;
		void operator =(IRenderStage &) = delete;

	protected:
		// helper functions
		PipelineItem IterateRenderPass(unsigned int &length, const signed long int passLength,
			const std::function<void ()> &PassFinish, const std::function<RenderStageItem::Work (unsigned long rangeBegin, unsigned long rangeEnd)> &GetRenderRange) const;
		PipelineItem IterateRenderPass(unsigned int &length, const signed long int passLength,
			const RenderPasses::PassROPBinding<RenderPasses::StageRTBinding> *RTBinding, const RenderPasses::PassROPBinding<RenderPasses::StageZBinding> &ZBinding, const RenderPasses::StageOutput &output,
			const std::function<void ()> &PassFinish, const std::function<RenderStageItem::Work (unsigned long rangeBegin, unsigned long rangeEnd, const RenderPasses::RenderPass &renderPass)> &GetRenderRange) const;

	private:
		// TODO: use C++20 abbreviated function template
		template<typename PassExhausted, typename GetRenderRange>
		inline PipelineItem IterateRenderPass(unsigned int &length, const signed long int passLength,
			const std::function<void ()> &PassFinish, const PassExhausted &PassExhausted, const GetRenderRange &GetRenderRange) const;

	public:
		void Sync() const { phaseSelector = DoSync(); }
		PipelineItem GetNextWorkItem(unsigned int &length) const { return (this->*phaseSelector)(length); }
	};
}