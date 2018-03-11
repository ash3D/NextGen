/**
\author		Alexey Shaydurov aka ASH
\date		11.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <functional>
#include "GPU work item.h"

namespace Renderer::Impl::RenderPipeline
{
	struct IRenderStage
	{
		virtual void Sync() const = 0;
		PipelineItem GetNextWorkItem(unsigned int &length) const { return (this->*getNextWorkItemSelector)(length); }

	protected:
		static PipelineItem (IRenderStage::*getNextWorkItemSelector)(unsigned int &length) const;

	protected:
		IRenderStage() = default;
		~IRenderStage() = default;
		IRenderStage(IRenderStage &) = delete;
		void operator =(IRenderStage &) = delete;

	protected:
		// helper function
		PipelineItem IterateRenderPass(unsigned int &length, const signed long int passLength,
			const std::function<void ()> &PassFinish, const std::function<RenderStageItem (unsigned long rangeBegin, unsigned long rangeEnd)> &GetRenderRange) const;
	};
}