/**
\author		Alexey Shaydurov aka ASH
\date		12.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#define RENDER_STAGE_INHERITANCE

#include <utility>
#include <variant>
#include <future>
#include "../GPU work item.h"
#if 0
#include "../render stage.h"
#elif defined _MSC_VER
// alternative is to use /vmg
#define RENDER_STAGE_INHERITANCE __single_inheritance
// !: check using forward declaration only with other compilers
#endif

struct ID3D12GraphicsCommandList1;

namespace Renderer::Impl::RenderPipeline
{
	typedef std::pair<const struct RENDER_STAGE_INHERITANCE IRenderStage *, PipelineItem (IRenderStage::*)(unsigned int &length) const> RenderStage;
	typedef std::variant<ID3D12GraphicsCommandList1 *, RenderStage> PipelineStage;

	void AppendStage(std::future<PipelineStage> &&stage);
	PipelineItem GetNext(unsigned int &length);
	void TerminateStageTraverse() noexcept;
	bool Empty() noexcept;
}

#undef RENDER_STAGE_INHERITANCE