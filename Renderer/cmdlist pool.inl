#pragma once

#include "stdafx.h"
#include "cmdlist pool.h"

namespace Renderer::Impl::CmdListPool
{
	inline CmdList::CmdList(CmdList &&src) : cmdCtx(src.cmdCtx), setup(src.setup), setupSimple(src.setupSimple), poolIdx(src.poolIdx), ringIdx(src.ringIdx)
	{
		src.cmdCtx = nullptr;
	}

	inline CmdList &CmdList::operator =(CmdList &&src)
	{
		cmdCtx = src.cmdCtx;
		setup = src.setup;
		setupSimple = src.setupSimple;
		poolIdx = src.poolIdx;
		ringIdx = src.ringIdx;
		src.cmdCtx = nullptr;
		return *this;
	}

	inline CmdList::operator ID3D12GraphicsCommandList4 *() const
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

	inline void CmdList::Setup()
	{
		assert(cmdCtx);
		(this->*setupSimple)(NULL);
	}

	inline void CmdList::MarkSuspended(bool suspended)
	{
		assert(cmdCtx);
		cmdCtx->suspended = suspended;
	}
}