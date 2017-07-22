/**
\author		Alexey Shaydurov aka ASH
\date		22.07.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "viewport.hh"
#include "world.hh"

using namespace std;
using namespace Renderer;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;

Impl::Viewport::EventHandle::EventHandle() : handle(CreateEvent(NULL, FALSE, FALSE, NULL))
{
	if (!handle)
		throw HRESULT_FROM_WIN32(GetLastError());
}

Impl::Viewport::EventHandle::~EventHandle()
{
	if (!CloseHandle(handle))
		cerr << "Fail to close event handle (hr=" << HRESULT_FROM_WIN32(GetLastError()) << ")." << endl;
}

Impl::Viewport::Viewport(shared_ptr<const Renderer::World> world) : world(move(world)), viewXform(), projXform()
{
	CheckHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocsStore[0])));
	CheckHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocsStore[0].Get(), NULL, IID_PPV_ARGS(&cmdList)));

	viewXform[0][0] = viewXform[1][1] = viewXform[2][2] = projXform[0][0] = projXform[1][1] = projXform[2][2] = projXform[3][3] = 1.f;
}

Impl::Viewport::~Viewport()
{
	WaitForGPU();
}

void Impl::Viewport::SetViewTransform(const float (&matrix)[4][3])
{
	memcpy(viewXform, matrix, sizeof viewXform);
}

void Impl::Viewport::SetProjectionTransform(double fovy, double zn, double zf)
{
	assert(isgreater(fovy, 0.));
	assert(isgreater(zn, 0.));
	assert(isgreater(zf, 0.));
	assert(isless(zn, zf));

	const double yScale = 1. / tan(fovy / 2.), invAspect = double(projXform[0][0]) / double(projXform[1][1]), zScale = isfinite(zf) ? zf / (zf - zn) : 1.;
	projXform[0][0] = yScale * invAspect;
	projXform[1][1] = yScale;
	projXform[2][2] = zScale;
	projXform[3][2] = -zn * zScale;
	projXform[2][3] = 1;
	projXform[3][3] = 0;
}

void Impl::Viewport::UpdateAspect(double invAspect)
{
	projXform[0][0] = projXform[1][1] * invAspect;
}

void Impl::Viewport::Render(ID3D12Resource *rt, const D3D12_CPU_DESCRIPTOR_HANDLE &rtv, UINT width, UINT height) const
{
	extern ComPtr<ID3D12CommandQueue> cmdQueue;
	if (fenceValue++)
	{
		// prepare command allocator
		const auto oldestFenceValue = fenceValue - cmdAllocCount;
		if (fence->GetCompletedValue() < oldestFenceValue)
		{
			if (cmdAllocCount < cmdListOverlapLimit)
			{
				const auto begin = cmdAllocsStore + curCmdAllocIdx, end = cmdAllocsStore + cmdAllocCount++;
				move_backward(begin, end, end + 1);
				CheckHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(begin->ReleaseAndGetAddressOf())));
			}
			else
			{
				WaitForGPU(oldestFenceValue);
				CheckHR(cmdAllocsStore[curCmdAllocIdx]->Reset());
			}
		}
		else
			CheckHR(cmdAllocsStore[curCmdAllocIdx]->Reset());

		CheckHR(cmdList->Reset(cmdAllocsStore[curCmdAllocIdx].Get(), NULL));
		++curCmdAllocIdx %= cmdAllocCount;
	}

	D3D12_RESOURCE_BARRIER rtBarrier = CD3DX12_RESOURCE_BARRIER::Transition(rt, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList->ResourceBarrier(1, &rtBarrier);
	const float color[4] = { .0f, .2f, .4f, 1.f };
	cmdList->ClearRenderTargetView(rtv, color, 0, NULL);
	cmdList->OMSetRenderTargets(1, &rtv, TRUE, NULL);
	const CD3DX12_VIEWPORT viewport(0.f, 0.f, width, height);
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, width, height));
	if (world)
		world->Render(viewXform, projXform, cmdList.Get(), curCmdAllocIdx);
	swap(rtBarrier.Transition.StateBefore, rtBarrier.Transition.StateAfter);
	cmdList->ResourceBarrier(1, &rtBarrier);
	CheckHR(cmdList->Close());
	ID3D12CommandList *const listToExecute = cmdList.Get();
	cmdQueue->ExecuteCommandLists(1, &listToExecute);
	//CheckHR(cmdQueue->Signal(fence.Get(), fenceValue));
}

void Impl::Viewport::Signal() const
{
	extern ComPtr<ID3D12CommandQueue> cmdQueue;
	CheckHR(cmdQueue->Signal(fence.Get(), fenceValue));
}

void Impl::Viewport::WaitForGPU(UINT64 waitFenceValue) const
{
	CheckHR(fence->SetEventOnCompletion(waitFenceValue, fenceEvent));
	if (WaitForSingleObject(fenceEvent, INFINITE) == WAIT_FAILED)
		throw HRESULT_FROM_WIN32(GetLastError());
}