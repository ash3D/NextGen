#pragma once

#include <memory>
#include "../tracked resource.h"
#include "../tracked ref.h"
#include "../tonemap resource views stage.h"

struct IDXGISwapChain4;
struct ID3D12Resource;
struct ID3D12DescriptorHeap;

extern void __cdecl InitRenderer();

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	class RenderOutput
	{
		Impl::TrackedResource<IDXGISwapChain4> swapChain;
		Impl::TrackedResource<ID3D12Resource> rendertarget, ZBuffer, HDRSurface, LDRSurface;
		Impl::TrackedResource<ID3D12DescriptorHeap> rtvHeap, dsvHeap;	// is tracking really needed?
		Impl::Descriptors::TonemapResourceViewsStage tonemapViewsCPUHeap;
		Impl::TrackedRef::Ref<class Viewport> viewport;

	private:
		friend extern void __cdecl ::InitRenderer();
		static WRL::ComPtr<ID3D12Resource> tonemapReductionBuffer, CreateTonemapReductionBuffer();

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
		void CreateOffscreenSurfaces(UINT width, UINT height);
	};
}