/**
\author		Alexey Shaydurov aka ASH
\date		25.04.2018 (c)Korotkov Andrey

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
		Impl::CmdCtx *cmdCtx;
		void (CmdList::*setup)(ID3D12PipelineState *PSO) = &CmdList::Init;
		size_t poolIdx;	// for name generation

	public:
		explicit CmdList();
		inline CmdList(CmdList &&src);
		inline CmdList &operator =(CmdList &&src);

	public:
		inline operator ID3D12GraphicsCommandList2 *() const;
		inline const auto &operator ->() const;
		inline void Setup(ID3D12PipelineState *PSO = NULL);

	public:
		void ResourceBarrier(const D3D12_RESOURCE_BARRIER &barrier), ResourceBarrier(std::initializer_list<D3D12_RESOURCE_BARRIER> barriers);
		void FlushBarriers();

	private:
		void Init(ID3D12PipelineState *PSO), Update(ID3D12PipelineState *PSO);
	};

	void OnFrameFinish();
}