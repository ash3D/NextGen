/**
\author		Alexey Shaydurov aka ASH
\date		15.11.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "render output.hh"
#include "viewport.hh"
#include "frame versioning.h"
#include "cmdlist pool.h"

using namespace std;
using namespace Renderer;
using Impl::globalFrameVersioning;
using WRL::ComPtr;

extern ComPtr<IDXGIFactory5> factory;
extern ComPtr<ID3D12Device2> device;
extern ComPtr<ID3D12CommandQueue> cmdQueue;

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
			0, 0,								// take width and height from wnd
			DXGI_FORMAT_R8G8B8A8_UNORM,			// format
			FALSE,								// stereo
			{1, 0},								// MSAA
			DXGI_USAGE_RENDER_TARGET_OUTPUT,	// usage
			bufferCount,						// buffer count
			DXGI_SCALING_STRETCH,				// scaling
			DXGI_SWAP_EFFECT_FLIP_DISCARD,		// swap effect
			DXGI_ALPHA_MODE_UNSPECIFIED,		// alpha mode
			flags
		};

		ComPtr<IDXGISwapChain1> swapChain;
		CheckHR(factory->CreateSwapChainForHwnd(cmdQueue.Get(), wnd, &desc, NULL, NULL, &swapChain));
		CheckHR(swapChain.As(&this->swapChain));
	}

	// create and populate rtv desctriptor heap
	{
		const D3D12_DESCRIPTOR_HEAP_DESC desc =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			bufferCount,
			D3D12_DESCRIPTOR_HEAP_FLAG_NONE
		};

		CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap)));

		Fill_RTV_Heap(bufferCount);
	}
}

RenderOutput::RenderOutput(const RenderOutput &) = default;
RenderOutput::RenderOutput(RenderOutput &&) = default;
RenderOutput &RenderOutput::operator =(const RenderOutput &) = default;
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
		throw HRESULT_FROM_WIN32(GetLastError());
	const unsigned long int newWidth = newWndRect.right - newWndRect.left, newHeight = newWndRect.bottom - newWndRect.top;

	// resize if necessarily or in case of significant shrink, modify source region otherwise
	if (newWidth > swapChainDesc.Width || newHeight > swapChainDesc.Height || newWidth * newHeight + resizeThresholdPixels < (unsigned long)swapChainDesc.Width * (unsigned long)swapChainDesc.Height)
	{
		/*
		wait until GPU has finished using swap chain buffers as rendertargets
		TODO: invistigate if it worth to wait
		- until Present() (it seems that without it device gets hung sometimes)
		- for the case of modification source region via SetSourceSize()
		*/
		globalFrameVersioning->WaitForGPU();

		vector<UINT> nodeMasks(swapChainDesc.BufferCount);
		vector<IUnknown *> cmdQueues(swapChainDesc.BufferCount, cmdQueue.Get());
		CheckHR(swapChain->ResizeBuffers1(swapChainDesc.BufferCount, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, swapChainDesc.Flags, nodeMasks.data(), cmdQueues.data()));
		Fill_RTV_Heap(swapChainDesc.BufferCount);
	}
	else
		CheckHR(swapChain->SetSourceSize(newWidth, newHeight));

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
	ComPtr<ID3D12Resource> rt;
	CheckHR(swapChain->GetBuffer(idx, IID_PPV_ARGS(&rt)));
	const auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	globalFrameVersioning->OnFrameStart();
	viewport->Render(rt.Get(), CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), idx, rtvDescriptorSize), width, height);
	CheckHR(swapChain->Present(vsync, 0));
	globalFrameVersioning->OnFrameFinish();
	CmdListPool::OnFrameFinish();
	OnFrameFinish();
}

void RenderOutput::Fill_RTV_Heap(unsigned int bufferCount)
{
	const auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (unsigned i = 0; i < bufferCount; i++, rtvHandle.Offset(rtvDescriptorSize))
	{
		ComPtr<ID3D12Resource> rt;
		CheckHR(swapChain->GetBuffer(i, IID_PPV_ARGS(&rt)));
		device->CreateRenderTargetView(rt.Get(), NULL, rtvHandle);
	}
}