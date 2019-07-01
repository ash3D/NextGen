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
	struct CmdCtx : private CmdBuffer<>
	{
		friend class CmdListPool::CmdList;

	private:
		std::vector<D3D12_RESOURCE_BARRIER> pendingBarriers;
	};
}