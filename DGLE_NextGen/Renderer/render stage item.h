/**
\author		Alexey Shaydurov aka ASH
\date		24.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <functional>

namespace Renderer::CmdListPool
{
	class CmdList;
}

namespace Renderer::Impl::RenderPipeline
{
	typedef std::function<void (CmdListPool::CmdList &)> RenderStageItem;
}