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

static remove_reference_t<decltype(globalFrameVersioning->GetCurFrameDataVersion())>::size_type firstFreeAllocIdx;
static vector<ComPtr<ID3D12GraphicsCommandList1>> lstPool;
static decltype(lstPool)::size_type firstUsedListIdx;

CmdList::CmdList() : allocIdx(firstFreeAllocIdx++), listIdx(firstUsedListIdx ? --firstUsedListIdx : lstPool.size())
{
	auto &curFrameAllocPool = globalFrameVersioning->GetCurFrameDataVersion();
	if (curFrameAllocPool.size() < firstFreeAllocIdx)
		curFrameAllocPool.emplace_back();
	if (listIdx == lstPool.size())
		lstPool.emplace_back();
}

CmdList::CmdList(CmdList &&src) : allocIdx(src.allocIdx), listIdx(src.listIdx), setup(src.setup)
{
	src.setup = nullptr;
}

CmdList &CmdList::operator =(CmdList &&src)
{
	CmdList temp(move(src));
	swap(allocIdx, temp.allocIdx);
	swap(listIdx, temp.listIdx);
	swap(setup, temp.setup);
	return *this;
}

CmdList::~CmdList()
{
	if (setup)
		lstPool[listIdx].Swap(lstPool[firstUsedListIdx++]);
}

CmdList::operator ID3D12GraphicsCommandList1 *() const
{
	assert(lstPool[listIdx]);
	return lstPool[listIdx].Get();
}

void CmdList::Init(ID3D12PipelineState *PSO)
{
	extern ComPtr<ID3D12Device2> device;

	// get allocator
	auto &alloc = globalFrameVersioning->GetCurFrameDataVersion()[allocIdx];
	if (alloc)
		CheckHR(alloc->Reset());
	else
		CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(alloc.GetAddressOf())));

	// get list
	auto &lst = lstPool[listIdx];
	if (lst)
		CheckHR(lst->Reset(alloc.Get(), PSO));
	else
		CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), PSO, IID_PPV_ARGS(lst.GetAddressOf())));

	setup = &CmdList::Update;
}

void CmdList::Update(ID3D12PipelineState *PSO)
{
	if (PSO)
		operator ID3D12GraphicsCommandList1 *()->SetPipelineState(PSO);
}

void CmdListPool::OnFrameFinish()
{
	firstFreeAllocIdx = 0;
}