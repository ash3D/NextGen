#include "stdafx.h"
#include "frame versioning.h"

using namespace std;
using namespace Renderer::Impl;
using WRL::ComPtr;

FrameVersioningBase::FrameVersioningBase(LPCWSTR objectName)
{
	extern ComPtr<ID3D12Device4> device;
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
	CheckHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	NameObjectF(fence.Get(), L"%ls fence", objectName);
}

FrameVersioningBase::~FrameVersioningBase() = default;

UINT64 FrameVersioningBase::GetCompletedFrameID() const
{
	return fence->GetCompletedValue();
}

void FrameVersioningBase::WaitForGPU(UINT64 waitFrameID) const
{
	CheckHR(fence->SetEventOnCompletion(waitFrameID, fenceEvent));
	if (WaitForSingleObject(fenceEvent, INFINITE) == WAIT_FAILED)
		throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
}

unsigned short FrameVersioningBase::OnFrameStart()
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

void FrameVersioningBase::OnFrameFinish()
{
	extern ComPtr<ID3D12CommandQueue> gfxQueue;
	CheckHR(gfxQueue->Signal(fence.Get(), frameID));
	++ringBufferIdx %= frameLatency;
}