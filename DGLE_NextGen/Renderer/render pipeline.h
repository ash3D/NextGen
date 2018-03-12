/**
\author		Alexey Shaydurov aka ASH
\date		12.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <utility>
#include <variant>
#include <future>
#include "../GPU work item.h"
#if 1
// !: fail at runtime on MSVC if forward declaration only used, further investigation needed (other compilers ?)
#include "../render stage.h"
#endif

struct ID3D12GraphicsCommandList1;

namespace Renderer::Impl::RenderPipeline
{
	typedef std::pair<const struct IRenderStage *, PipelineItem (IRenderStage::*)(unsigned int &length) const> RenderStage;
	typedef std::variant<ID3D12GraphicsCommandList1 *, RenderStage> PipelineStage;

	void AppendStage(std::future<PipelineStage> &&stage);
	PipelineItem GetNext(unsigned int &length);
	void TerminateStageTraverse() noexcept;
	bool Empty() noexcept;
}