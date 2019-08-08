#pragma once

#define NOMINMAX

#include <algorithm>
#include <deque>
#include <optional>
#include <wrl/client.h>
#include "../event handle.h"
#include "../cmdlist pool.h"

struct ID3D12Fence;

namespace Renderer::Impl
{
	namespace WRL = Microsoft::WRL;

	static constexpr unsigned short maxFrameLatency = 3;

	class FrameVersioningBase
	{
		UINT64 frameID = 0;
		EventHandle fenceEvent;
		WRL::ComPtr<ID3D12Fence> fence;
		unsigned short frameLatency = 1, ringBufferIdx = 0;

	protected:
		FrameVersioningBase(LPCWSTR objectName);
		FrameVersioningBase(FrameVersioningBase &) = delete;
		FrameVersioningBase &operator =(FrameVersioningBase &) = delete;
		~FrameVersioningBase();

	public:
		// frameLatency corresponds to ringBuffer size, maxFrameLatency - to its capacity
		unsigned short GetFrameLatency() const noexcept { return frameLatency; }
		unsigned short GetContinuousRingIdx() const noexcept { return (frameID - 1) % maxFrameLatency; }
		UINT64 GetCurFrameID() const noexcept { return frameID; }
		UINT64 GetCompletedFrameID() const;
		void WaitForGPU() const { WaitForGPU(frameID); }

	private:
		void WaitForGPU(UINT64 waitFrameID) const;

	protected:
		unsigned short GetRingBufferIdx() const noexcept { return ringBufferIdx; }
		unsigned short OnFrameStart();

	public:
		void OnFrameFinish();
	};

	template<class Data, LPCWSTR objectName>
	class FrameVersioning : public FrameVersioningBase
	{
		Data ringBuffer[maxFrameLatency];

	public:
		FrameVersioning() : FrameVersioningBase(objectName) {}
		~FrameVersioning() { WaitForGPU(); }

	public:
		void OnFrameStart();
		Data &GetCurFrameDataVersion() noexcept { return ringBuffer[GetRingBufferIdx()]; }

	private:
		// hide from protected
		using FrameVersioningBase::GetRingBufferIdx;
	};

	// Data here is a cmd ctx pool
	extern inline constexpr const WCHAR globalFrameVersioningName[] = L"global frame versioning";
	extern std::optional<FrameVersioning<CmdListPool::PerFramePool, globalFrameVersioningName>> globalFrameVersioning;

	// template impl
	template<class Data, LPCWSTR objectName>
	void FrameVersioning<Data, objectName>::OnFrameStart()
	{
		if (const unsigned short rangeEndIdx = FrameVersioningBase::OnFrameStart())
		{
			const auto rangeBegin = ringBuffer + GetRingBufferIdx(), rangeEnd = ringBuffer + rangeEndIdx;
			std::move_backward(rangeBegin, rangeEnd, rangeEnd + 1);
		}
	}
}