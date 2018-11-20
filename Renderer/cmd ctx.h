/**
\author		Alexey Shaydurov aka ASH
\date		25.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <vector>
#include "../cmd buffer.h"

struct D3D12_RESOURCE_BARRIER;

namespace Renderer::CmdListPool
{
	class CmdList;
}

namespace Renderer::Impl
{
	struct CmdCtx : private CmdBuffer
	{
		friend class CmdListPool::CmdList;

	private:
		std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers;
	};
}