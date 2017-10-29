/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "frame versioning.h"

using namespace std;
using namespace Renderer::Impl;
using WRL::ComPtr;

FrameVersioning<void>::EventHandle::EventHandle() : handle(CreateEvent(NULL, FALSE, FALSE, NULL))
{
	if (!handle)
		throw HRESULT_FROM_WIN32(GetLastError());
}

FrameVersioning<void>::EventHandle::~EventHandle()
{
	if (!CloseHandle(handle))
		cerr << "Fail to close event handle (hr=" << HRESULT_FROM_WIN32(GetLastError()) << ")." << endl;
}

FrameVersioning<void>::FrameVersioning()
{
	extern ComPtr<ID3D12Device2> device;
	CheckHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
}

FrameVersioning<void>::~FrameVersioning()
{
	WaitForGPU();
}

UINT64 FrameVersioning<void>::GetCompletedFrameID() const
{
	return fence->GetCompletedValue();
}

void FrameVersioning<void>::WaitForGPU(UINT64 waitFrameID) const
{
	CheckHR(fence->SetEventOnCompletion(waitFrameID, fenceEvent));
	if (WaitForSingleObject(fenceEvent, INFINITE) == WAIT_FAILED)
		throw HRESULT_FROM_WIN32(GetLastError());
}

unsigned short FrameVersioning<void>::OnFrameStart()
{
	const auto oldestTrackedFrame = ++frameID - frameLatency;
	if (fence->GetCompletedValue() < oldestTrackedFrame)
	{
		if (frameLatency < maxFrameLatency)
			return frameLatency++;
		else
			WaitForGPU(oldestTrackedFrame);
	}
	return 0;
}

void FrameVersioning<void>::OnFrameFinish()
{
	extern ComPtr<ID3D12CommandQueue> cmdQueue;
	CheckHR(cmdQueue->Signal(fence.Get(), frameID));
	++ringBufferIdx %= frameLatency;
}