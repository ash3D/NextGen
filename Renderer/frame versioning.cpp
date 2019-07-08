#include "stdafx.h"
#include "frame versioning.h"

using namespace std;
using namespace Renderer::Impl;
using WRL::ComPtr;

FrameVersioning<void>::FrameVersioning()
{
	extern ComPtr<ID3D12Device2> device;
	CheckHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
}

FrameVersioning<void>::~FrameVersioning() = default;

UINT64 FrameVersioning<void>::GetCompletedFrameID() const
{
	return fence->GetCompletedValue();
}

void FrameVersioning<void>::WaitForGPU(UINT64 waitFrameID) const
{
	CheckHR(fence->SetEventOnCompletion(waitFrameID, fenceEvent));
	if (WaitForSingleObject(fenceEvent, INFINITE) == WAIT_FAILED)
		throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
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
	extern ComPtr<ID3D12CommandQueue> gfxQueue;
	CheckHR(gfxQueue->Signal(fence.Get(), frameID));
	++ringBufferIdx %= frameLatency;
}