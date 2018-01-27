/**
\author		Alexey Shaydurov aka ASH
\date		27.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <memory>
#include "../tracked resource.h"
#include "../tracked ref.h"

struct IDXGISwapChain4;
struct ID3D12Resource;
struct ID3D12DescriptorHeap;

namespace Renderer
{
	class RenderOutput
	{
		Impl::TrackedResource<IDXGISwapChain4> swapChain;
		Impl::TrackedResource<ID3D12Resource> ZBuffer;
		Impl::TrackedResource<ID3D12DescriptorHeap> rtvHeap, dsvHeap;	// is tracking really needed?
		Impl::TrackedRef::Ref<class Viewport> viewport;

	public:
		RenderOutput(HWND wnd, bool allowModeSwitch, unsigned int bufferCount = 2);
		RenderOutput(RenderOutput &&);
		RenderOutput &operator =(RenderOutput &&);
		~RenderOutput();

	public:
		void Monitor_ALT_ENTER(bool enable);
		void SetViewport(std::shared_ptr<Viewport> viewport);
		void Resize(unsigned int width, unsigned int height), OnResize();
		void NextFrame(bool vsync = true);

	private:
		void Fill_RTV_Heap(unsigned int bufferCount);
		void CreateZBuffer(UINT width, UINT height);
	};
}