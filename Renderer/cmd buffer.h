#pragma once

#define NOMINMAX

#include <wrl/client.h>

struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList2;

namespace Renderer::Impl
{
	namespace WRL = Microsoft::WRL;

	struct CmdBuffer
	{
		WRL::ComPtr<ID3D12CommandAllocator> allocator;
		WRL::ComPtr<ID3D12GraphicsCommandList2> list;
	};
}