#pragma once

#include <memory>
#include "../tracked resource.h"
#include "../tracked ref.h"
#include "../postprocess descriptor table store.h"

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
		Impl::TrackedResource<ID3D12Resource> rendertarget, ZBuffer, HDRSurfaces[2], LDRSurface, COCBuffer, halfresDOFSurface/*scene color + CoC*/, DOFLayers, lensFlareSurface, bloomUpChain, bloomDownChain;
		Impl::TrackedResource<ID3D12DescriptorHeap> rtvHeap, dsvHeap;	// is tracking really needed?
		Impl::Descriptors::PostprocessDescriptorTableStore postprocessCPUDescriptorHeap;
		Impl::TrackedRef::Ref<class Viewport> viewport;

	private:
		enum
		{
			SCENE_RTV,
			DOF_LAYERS_RTVs,
			DOF_NEAR_RTV = DOF_LAYERS_RTVs,
			DOF_FAR_RTV,
			LENS_FLARE_RTV,
			RTV_COUNT
		};

	private:
		friend extern void __cdecl ::InitRenderer();
		static WRL::ComPtr<ID3D12Resource> luminanceReductionBuffer, CreateLuminanceReductionBuffer();

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