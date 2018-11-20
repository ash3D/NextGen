#pragma once

#include "stdafx.h"

namespace Renderer::Impl::Descriptors
{
	namespace WRL = Microsoft::WRL;
	class TonemapResourceViewsStage;
}

namespace Renderer::Impl::Descriptors::GPUDescriptorHeap
{
	namespace Impl
	{
		extern WRL::ComPtr<ID3D12DescriptorHeap> CreateHeap(), heap;
	}

	inline const WRL::ComPtr<ID3D12DescriptorHeap> &GetHeap() noexcept { return Impl::heap; }
	D3D12_GPU_DESCRIPTOR_HANDLE SetCurFrameTonemapReductionDescs(const TonemapResourceViewsStage &src, UINT backBufferIdx);
}