#pragma once

#define NOMINMAX

//#include <type_traits>
#include <wrl/client.h>

struct ID3D12CommandAllocator;
struct ID3D12CommandList;
struct ID3D12GraphicsCommandList2;

namespace Renderer::Impl
{
	namespace WRL = Microsoft::WRL;

	template<class CmdList = ID3D12GraphicsCommandList2>
	struct CmdBuffer
	{
		//static_assert(std::is_base_of_v<ID3D12CommandList, CmdList>);
		WRL::ComPtr<ID3D12CommandAllocator> allocator;
		WRL::ComPtr<CmdList> list;
	};
}