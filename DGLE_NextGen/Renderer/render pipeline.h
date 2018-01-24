/**
\author		Alexey Shaydurov aka ASH
\date		24.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "render stage item.h"

namespace Renderer::Impl::RenderPipeline
{
	typedef std::variant<ID3D12GraphicsCommandList1 *, const struct IRenderStage *> PipelineStage;

	void AppendStage(std::future<PipelineStage> &&stage);
	std::variant<std::monostate, ID3D12GraphicsCommandList1 *, RenderStageItem> GetNext(unsigned int &length);
	void TerminateStageTraverse() noexcept;
	bool Empty() noexcept;
}