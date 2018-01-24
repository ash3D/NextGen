/**
\author		Alexey Shaydurov aka ASH
\date		24.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <functional>
#include "render stage item.h"

namespace Renderer::Impl::RenderPipeline
{
	struct IRenderStage
	{
		virtual void Sync() const = 0;
		virtual RenderStageItem GetNextRenderItem(unsigned int &length) const = 0;

	protected:
		IRenderStage() = default;
		~IRenderStage() = default;
		IRenderStage(IRenderStage &) = delete;
		void operator =(IRenderStage &) = delete;

	protected:
		// helper function
		RenderStageItem IterateRenderPass(unsigned int &length, const signed long int passLength,
			const std::function<void ()> &NextStageItem, const std::function<RenderStageItem (unsigned long rangeBegin, unsigned long rangeEnd)> &GetRenderRange) const;
	};
}