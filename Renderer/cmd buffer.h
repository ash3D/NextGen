/**
\author		Alexey Shaydurov aka ASH
\date		25.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

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