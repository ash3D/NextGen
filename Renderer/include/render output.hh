#pragma once

#include <memory>
#include "../tracked resource.h"
#include "../tracked ref.h"
#include "../offscreen buffers.h"

struct IDXGISwapChain4;

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	class RenderOutput
	{
		Impl::TrackedResource<IDXGISwapChain4> swapChain;
		Impl::OffscreenBuffers::Handle offscreenBuffers;
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
	};
}