/**
\author		Alexey Shaydurov aka ASH
\date		09.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <functional>
#include <variant>

struct ID3D12GraphicsCommandList1;

namespace Renderer::CmdListPool
{
	class CmdList;
}

namespace Renderer::Impl::RenderPipeline
{
	typedef std::function<void (CmdListPool::CmdList &)> RenderStageItem;
	typedef std::variant<std::monostate, ID3D12GraphicsCommandList1 *, RenderStageItem> PipelineItem;
}