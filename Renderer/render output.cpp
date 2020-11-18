#include "stdafx.h"
#include "render output.hh"
#include "viewport.hh"
#include "tracked resource.inl"
#include "tracked ref.inl"
#include "frame versioning.h"
#include "cmdlist pool.h"
#include "GPU descriptor heap.h"
#include "config.h"

// workaround for Kepler driver issue causing sporadic device removal on first present in case of complex frame
#define ENABLE_SWAPCHAIN_WARMUP 1

using namespace std;
using namespace Renderer;
using Impl::globalFrameVersioning;
using WRL::ComPtr;

extern ComPtr<IDXGIFactory5> factory;
extern ComPtr<ID3D12Device4> device;
extern ComPtr<ID3D12CommandQueue> gfxQueue;

constexpr unsigned long int resizeThresholdPixels = 100'000ul;

RenderOutput::RenderOutput(HWND wnd, bool allowModeSwitch, unsigned int bufferCount)
{
	// create swap chain
	{
		UINT flags = 0;
		if (allowModeSwitch)
			flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		const DXGI_SWAP_CHAIN_DESC1 desc =
		{
			0, 0,							// take width and height from wnd
			Config::DisplayFormat,			// format
			FALSE,							// stereo
			{1, 0},							// MSAA
			0,								// usage
			bufferCount,					// buffer count
			DXGI_SCALING_STRETCH,			// scaling
			DXGI_SWAP_EFFECT_FLIP_DISCARD,	// swap effect
			DXGI_ALPHA_MODE_UNSPECIFIED,	// alpha mode
			flags
		};

		ComPtr<IDXGISwapChain1> swapChain;
		CheckHR(factory->CreateSwapChainForHwnd(gfxQueue.Get(), wnd, &desc, NULL, NULL, &swapChain));
		CheckHR(swapChain.As(&this->swapChain));
#if ENABLE_SWAPCHAIN_WARMUP
		CheckHR(swapChain->Present(0, 0));
#endif
	}

	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		CheckHR(swapChain->GetDesc1(&swapChainDesc));
		offscreenBuffers = Impl::OffscreenBuffers::Create(swapChainDesc.Width, swapChainDesc.Height);
	}
}

RenderOutput::RenderOutput(RenderOutput &&) = default;
RenderOutput &RenderOutput::operator =(RenderOutput &&) = default;
RenderOutput::~RenderOutput() = default;

void RenderOutput::Monitor_ALT_ENTER(bool enable)
{
	HWND wnd;
	CheckHR(swapChain->GetHwnd(&wnd));
	CheckHR(factory->MakeWindowAssociation(wnd, enable ? 0 : DXGI_MWA_NO_ALT_ENTER));
}

void RenderOutput::SetViewport(shared_ptr<Viewport> viewport)
{
	if (!viewport)
		throw logic_error("Attempt to attach null viewport.");

	UINT width, height;
	CheckHR(swapChain->GetSourceSize(&width, &height));

	(this->viewport = move(viewport))->UpdateAspect(double(height) / double(width));
}

void RenderOutput::Resize(unsigned int width, unsigned int height)
{
	DXGI_SWAP_CHAIN_DESC desc;
	CheckHR(swapChain->GetDesc(&desc));
	desc.BufferDesc.Width = width;
	desc.BufferDesc.Height = height;
	CheckHR(swapChain->ResizeTarget(&desc.BufferDesc));
	OnResize();
}

void RenderOutput::OnResize()
{
	UINT oldWidth, oldHeight;
	CheckHR(swapChain->GetSourceSize(&oldWidth, &oldHeight));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	CheckHR(swapChain->GetDesc1(&swapChainDesc));

	HWND wnd;
	CheckHR(swapChain->GetHwnd(&wnd));
	RECT newWndRect;
	if (!GetWindowRect(wnd, &newWndRect))
		throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
	const unsigned long int newWidth = newWndRect.right - newWndRect.left, newHeight = newWndRect.bottom - newWndRect.top;

	// resize if necessarily or in case of significant shrink, modify source region otherwise
	const bool recreateBuffers = newWidth > swapChainDesc.Width || newHeight > swapChainDesc.Height || newWidth * newHeight + resizeThresholdPixels < (unsigned long)swapChainDesc.Width *(unsigned long)swapChainDesc.Height;
	if (recreateBuffers)
	{
		/*
		wait until GPU has finished using swap chain buffers as rendertargets
		TODO: investigate if it worth to wait
		- until Present() (it seems that without it device gets hung sometimes)
		- for the case of modification source region via SetSourceSize()
		*/
		globalFrameVersioning->WaitForGPU();

		const vector<UINT> nodeMasks(swapChainDesc.BufferCount);
		const vector<IUnknown *> cmdQueues(swapChainDesc.BufferCount, gfxQueue.Get());
		CheckHR(swapChain->ResizeBuffers1(swapChainDesc.BufferCount, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, swapChainDesc.Flags, nodeMasks.data(), cmdQueues.data()));
	}
	else
		CheckHR(swapChain->SetSourceSize(newWidth, newHeight));

	offscreenBuffers->Resize(newWidth, newHeight, recreateBuffers);

	if (viewport)
		viewport->UpdateAspect(double(newHeight) / double(newWidth));
}

void RenderOutput::NextFrame(bool vsync)
{
	extern void OnFrameFinish();

	if (!viewport)
		throw logic_error("Attempt to render without viewport being attached.");

	UINT width, height;
	CheckHR(swapChain->GetSourceSize(&width, &height));
	const auto idx = swapChain->GetCurrentBackBufferIndex();
	ComPtr<ID3D12Resource> output;
	CheckHR(swapChain->GetBuffer(idx, IID_PPV_ARGS(&output)));
	Impl::Descriptors::GPUDescriptorHeap::OnFrameStart();
	globalFrameVersioning->OnFrameStart();
	viewport->Render(output.Get(), *offscreenBuffers, width, height);
	CheckHR(swapChain->Present(vsync, 0));
	globalFrameVersioning->OnFrameFinish();
	viewport->OnFrameFinish();
	Impl::CmdListPool::OnFrameFinish();
	OnFrameFinish();
}