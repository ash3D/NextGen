/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"

// not thread-safe
namespace Renderer::CmdListPool
{
	class CmdList
	{
		size_t allocIdx, listIdx;
		void (CmdList::*setup)(ID3D12PipelineState *PSO) = &CmdList::Init;

	public:
		explicit CmdList();
		CmdList(CmdList &&src);
		CmdList &operator =(CmdList &&src);
		~CmdList();

	public:
		operator ID3D12GraphicsCommandList1 *() const;
		ID3D12GraphicsCommandList1 *operator ->() const { return *this; }
		void Setup(ID3D12PipelineState *PSO) { (this->*setup)(PSO); }

	private:
		void Init(ID3D12PipelineState *PSO), Update(ID3D12PipelineState *PSO);
	};

	void OnFrameFinish();
}