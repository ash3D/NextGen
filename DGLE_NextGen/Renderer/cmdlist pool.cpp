/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "cmdlist pool.h"
#include "frame versioning.h"

using namespace std;
using namespace Renderer;
using namespace CmdListPool;
using Impl::globalFrameVersioning;
using Microsoft::WRL::ComPtr;

static remove_reference_t<decltype(globalFrameVersioning->GetCurFrameDataVersion())>::size_type firstFreePoolIdx;

CmdList::CmdList() : poolIdx(firstFreePoolIdx++)
{
	auto &curFramePool = globalFrameVersioning->GetCurFrameDataVersion();
	if (curFramePool.size() < firstFreePoolIdx)
		curFramePool.emplace_back();
}

CmdList::operator ID3D12GraphicsCommandList1 *() const
{
	const auto &list = globalFrameVersioning->GetCurFrameDataVersion()[poolIdx].list;
	assert(list);
	return list.Get();
}

void CmdList::Init(ID3D12PipelineState *PSO)
{
	extern ComPtr<ID3D12Device2> device;

	auto &curFramePool = globalFrameVersioning->GetCurFrameDataVersion()[poolIdx];
	if (curFramePool.allocator && curFramePool.list)
	{
		// reset
		CheckHR(curFramePool.allocator->Reset());
		CheckHR(curFramePool.list->Reset(curFramePool.allocator.Get(), PSO));
	}
	else
	{
		// create
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(curFramePool.allocator.GetAddressOf())));
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, curFramePool.allocator.Get(), PSO, IID_PPV_ARGS(curFramePool.list.GetAddressOf())));
	}

	setup = &CmdList::Update;
}

void CmdList::Update(ID3D12PipelineState *PSO)
{
	if (PSO)
		operator ID3D12GraphicsCommandList1 *()->SetPipelineState(PSO);
}

void CmdListPool::OnFrameFinish()
{
	firstFreePoolIdx = 0;
}