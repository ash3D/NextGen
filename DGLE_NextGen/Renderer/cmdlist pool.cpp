/**
\author		Alexey Shaydurov aka ASH
\date		24.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "cmdlist pool.h"

using namespace std;
using namespace Renderer;
using namespace CmdListPool;
using Impl::globalFrameVersioning;
using Microsoft::WRL::ComPtr;

static remove_reference_t<decltype(globalFrameVersioning->GetCurFrameDataVersion())>::size_type firstFreePoolIdx;

CmdList::CmdList() : poolIdx(firstFreePoolIdx)
{
	auto &curFramePool = globalFrameVersioning->GetCurFrameDataVersion();
	cmdBuffer = curFramePool.size() <= firstFreePoolIdx ? &curFramePool.emplace_back() : &curFramePool[firstFreePoolIdx];
	firstFreePoolIdx++;	// increment after insertion improves exception safety guarantee
}

void CmdList::Init(ID3D12PipelineState *PSO)
{
	extern ComPtr<ID3D12Device2> device;
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

	if (cmdBuffer->allocator && cmdBuffer->list)
	{
		// reset
		CheckHR(cmdBuffer->allocator->Reset());
		CheckHR(cmdBuffer->list->Reset(cmdBuffer->allocator.Get(), PSO));
	}
	else
	{
		const unsigned short version = globalFrameVersioning->GetFrameLatency() - 1;

		// create
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdBuffer->allocator.GetAddressOf())));
		NameObjectF(cmdBuffer->allocator.Get(), L"pool command allocator [%hu][%zu]", version, poolIdx);
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdBuffer->allocator.Get(), PSO, IID_PPV_ARGS(cmdBuffer->list.GetAddressOf())));
		NameObjectF(cmdBuffer->list.Get(), L"pool command list [%hu][%zu]", version, poolIdx);
	}

	setup = &CmdList::Update;
}

void CmdList::Update(ID3D12PipelineState *PSO)
{
	operator ID3D12GraphicsCommandList1 *()->ClearState(PSO);
}

void CmdListPool::OnFrameFinish()
{
	firstFreePoolIdx = 0;
}