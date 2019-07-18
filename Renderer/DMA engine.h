#pragma once

#include "stdafx.h"
#include "cmd buffer.h"

namespace Renderer::DMA
{
	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		extern std::array<Renderer::Impl::CmdBuffer<ID3D12GraphicsCommandList>, 2> cmdBuffers, CreateCmdBuffers();
		extern WRL::ComPtr<ID3D12Fence> fence, CreateFence();
	}

	// replace vector with C++20 span
	void Upload2VRAM(const WRL::ComPtr<ID3D12Resource> &dst, const std::vector<D3D12_SUBRESOURCE_DATA> &src, LPCWSTR name);
	void TrackUsage(ID3D12Resource *res);
	void Sync();
}