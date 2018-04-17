/**
\author		Alexey Shaydurov aka ASH
\date		17.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "cmdlist pool.h"

namespace Renderer::CmdListPool
{
	inline CmdList::CmdList(CmdList &&src) : cmdBuffer(src.cmdBuffer), setup(src.setup), poolIdx(src.poolIdx)
	{
		src.cmdBuffer = nullptr;
	}

	inline CmdList &CmdList::operator =(CmdList &&src)
	{
		cmdBuffer = src.cmdBuffer;
		setup = src.setup;
		poolIdx = src.poolIdx;
		src.cmdBuffer = nullptr;
		return *this;
	}

	inline CmdList::operator ID3D12GraphicsCommandList2 *() const
	{
		assert(cmdBuffer);
		assert(cmdBuffer->list);
		return cmdBuffer->list.Get();
	}

	inline const auto &CmdList::operator ->() const
	{
		assert(cmdBuffer);
		assert(cmdBuffer->list);
		return cmdBuffer->list;
	}

	inline void CmdList::Setup(ID3D12PipelineState *PSO)
	{
		assert(cmdBuffer);
		(this->*setup)(PSO);
	}
}