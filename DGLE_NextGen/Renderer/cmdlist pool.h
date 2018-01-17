/**
\author		Alexey Shaydurov aka ASH
\date		17.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "frame versioning.h"

// not thread-safe
namespace Renderer::CmdListPool
{
	class CmdList
	{
		Impl::CmdBuffer *cmdBuffer;
		void (CmdList::*setup)(ID3D12PipelineState *PSO) = &CmdList::Init;
		size_t poolIdx;	// for name generation

	public:
		explicit CmdList();
		CmdList(CmdList &&src);
		CmdList &operator =(CmdList &&src);

	public:
		operator ID3D12GraphicsCommandList1 *() const;
		ID3D12GraphicsCommandList1 *operator ->() const { return *this; }
		void Setup(ID3D12PipelineState *PSO);

	private:
		void Init(ID3D12PipelineState *PSO), Update(ID3D12PipelineState *PSO);
	};

	void OnFrameFinish();

	// inline impl

	inline CmdList::CmdList(CmdList &&src) : cmdBuffer(src.cmdBuffer), setup(src.setup)
	{
		src.cmdBuffer = nullptr;
	}

	inline CmdList &CmdList::operator =(CmdList &&src)
	{
		cmdBuffer = src.cmdBuffer;
		setup = src.setup;
		src.cmdBuffer = nullptr;
		return *this;
	}

	inline void CmdList::Setup(ID3D12PipelineState *PSO)
	{
		assert(cmdBuffer);
		(this->*setup)(PSO);
	}
}