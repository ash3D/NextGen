/**
\author		Alexey Shaydurov aka ASH
\date		10.05.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "render output.hh"
#include "viewport.hh"
#include "tracked resource.inl"
#include "tracked ref.inl"
#include "frame versioning.h"
#include "cmdlist pool.h"
#include "config.h"

using namespace std;
using namespace Renderer;
using Impl::globalFrameVersioning;
using Microsoft::WRL::ComPtr;

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
			0, 0,							// take width and height from wnd
			Config::ColorFormat,			// format
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
		CheckHR(factory->CreateSwapChainForHwnd(cmdQueue.Get(), wnd, &desc, NULL, NULL, &swapChain));
		CheckHR(swapChain.As(&this->swapChain));
	}

	// create rtv desctriptor heap
	{
		const D3D12_DESCRIPTOR_HEAP_DESC desc =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			1,
			D3D12_DESCRIPTOR_HEAP_FLAG_NONE
		};

		CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(rtvHeap.GetAddressOf())));
	}

	// create dsv descriptor heap
	{
		const D3D12_DESCRIPTOR_HEAP_DESC desc =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			1,
			D3D12_DESCRIPTOR_HEAP_FLAG_NONE
		};

		CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(dsvHeap.GetAddressOf())));
	}

	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		CheckHR(swapChain->GetDesc1(&swapChainDesc));
		CreateOffscreenSurfaces(swapChainDesc.Width, swapChainDesc.Height);
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
		CreateOffscreenSurfaces(newWidth, newHeight);
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
	ComPtr<ID3D12Resource> output;
	CheckHR(swapChain->GetBuffer(idx, IID_PPV_ARGS(&output)));
	globalFrameVersioning->OnFrameStart();
	viewport->Render(output.Get(), rt.Get(), ZBuffer.Get(), rtvHeap->GetCPUDescriptorHandleForHeapStart(), dsvHeap->GetCPUDescriptorHandleForHeapStart(), width, height);
	CheckHR(swapChain->Present(vsync, 0));
	globalFrameVersioning->OnFrameFinish();
	viewport->OnFrameFinish();
	CmdListPool::OnFrameFinish();
	OnFrameFinish();
}

void RenderOutput::CreateOffscreenSurfaces(UINT width, UINT height)
{
	const DXGI_SAMPLE_DESC MSAA_mode = Config::MSAA();

	// create rendertarget
	extern const float backgroundColor[4];
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::ColorFormat, width, height, 1, 1, MSAA_mode.Count, MSAA_mode.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&CD3DX12_CLEAR_VALUE(Config::ColorFormat, backgroundColor),
		IID_PPV_ARGS(rt.ReleaseAndGetAddressOf())
	));

	// fill RTV heap
	device->CreateRenderTargetView(rt.Get(), NULL, rtvHeap->GetCPUDescriptorHandleForHeapStart());

	// create z/stencil buffer
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::ZFormat, width, height, 1, 1, MSAA_mode.Count, MSAA_mode.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&CD3DX12_CLEAR_VALUE(Config::ZFormat, 1.f, 0xef),
		IID_PPV_ARGS(ZBuffer.ReleaseAndGetAddressOf())
	));

	// fill DSV heap
	device->CreateDepthStencilView(ZBuffer.Get(), NULL, dsvHeap->GetCPUDescriptorHandleForHeapStart());
}