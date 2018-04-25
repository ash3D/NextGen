/**
\author		Alexey Shaydurov aka ASH
\date		25.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "cmdlist pool.h"
#include "cmd ctx.h"

namespace Renderer::CmdListPool
{
	inline CmdList::CmdList(CmdList &&src) : cmdCtx(src.cmdCtx), setup(src.setup), poolIdx(src.poolIdx)
	{
		src.cmdCtx = nullptr;
	}

	inline CmdList &CmdList::operator =(CmdList &&src)
	{
		cmdCtx = src.cmdCtx;
		setup = src.setup;
		poolIdx = src.poolIdx;
		src.cmdCtx = nullptr;
		return *this;
	}

	inline CmdList::operator ID3D12GraphicsCommandList2 *() const
	{
		assert(cmdCtx);
		assert(cmdCtx->list);
		return cmdCtx->list.Get();
	}

	inline const auto &CmdList::operator ->() const
	{
		assert(cmdCtx);
		assert(cmdCtx->list);
		return cmdCtx->list;
	}

	inline void CmdList::Setup(ID3D12PipelineState *PSO)
	{
		assert(cmdCtx);
		(this->*setup)(PSO);
	}
}