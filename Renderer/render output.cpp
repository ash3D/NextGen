#include "stdafx.h"
#include "render output.hh"
#include "viewport.hh"
#include "tracked resource.inl"
#include "tracked ref.inl"
#include "frame versioning.h"
#include "cmdlist pool.h"
#include "GPU descriptor heap.h"
#include "config.h"
#include "CS config.h"

// workaround for Kepler driver issue causing sporadic device removal on first present in case of complex frame
#define ENABLE_SWAPCHAIN_WARMUP 1

using namespace std;
using namespace Renderer;
using Impl::globalFrameVersioning;
using WRL::ComPtr;

extern ComPtr<IDXGIFactory5> factory;
extern ComPtr<ID3D12Device2> device;
extern ComPtr<ID3D12CommandQueue> gfxQueue;

void NameObject(ID3D12Object *object, LPCWSTR name) noexcept;

constexpr unsigned long int resizeThresholdPixels = 100'000ul;

ComPtr<ID3D12Resource> RenderOutput::CreateLuminanceReductionBuffer()
{
	ComPtr<ID3D12Resource> result;
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(float [2048][2]), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		NULL,
		IID_PPV_ARGS(result.GetAddressOf())
	));
	NameObject(result.Get(), L"luminance reduction buffer");

	return result;
}

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

	// create rtv descriptor heap
	{
		const D3D12_DESCRIPTOR_HEAP_DESC desc =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			RTV_COUNT,
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
		throw _com_error(HRESULT_FROM_WIN32(GetLastError()));
	const unsigned long int newWidth = newWndRect.right - newWndRect.left, newHeight = newWndRect.bottom - newWndRect.top;

	// resize if necessarily or in case of significant shrink, modify source region otherwise
	if (newWidth > swapChainDesc.Width || newHeight > swapChainDesc.Height || newWidth * newHeight + resizeThresholdPixels < (unsigned long)swapChainDesc.Width * (unsigned long)swapChainDesc.Height)
	{
		/*
		wait until GPU has finished using swap chain buffers as rendertargets
		TODO: investigate if it worth to wait
		- until Present() (it seems that without it device gets hung sometimes)
		- for the case of modification source region via SetSourceSize()
		*/
		globalFrameVersioning->WaitForGPU();

		vector<UINT> nodeMasks(swapChainDesc.BufferCount);
		vector<IUnknown *> cmdQueues(swapChainDesc.BufferCount, gfxQueue.Get());
		CheckHR(swapChain->ResizeBuffers1(swapChainDesc.BufferCount, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, swapChainDesc.Flags, nodeMasks.data(), cmdQueues.data()));
	}
	else
		CheckHR(swapChain->SetSourceSize(newWidth, newHeight));

	CreateOffscreenSurfaces(newWidth, newHeight);

	if (viewport)
		viewport->UpdateAspect(double(newHeight) / double(newWidth));
}

void RenderOutput::NextFrame(bool vsync)
{
	namespace GPUDescriptorHeap = Impl::Descriptors::GPUDescriptorHeap;
	extern void OnFrameFinish();

	if (!viewport)
		throw logic_error("Attempt to render without viewport being attached.");

	UINT width, height;
	CheckHR(swapChain->GetSourceSize(&width, &height));
	const auto idx = swapChain->GetCurrentBackBufferIndex();
	ComPtr<ID3D12Resource> output;
	CheckHR(swapChain->GetBuffer(idx, IID_PPV_ARGS(&output)));
	GPUDescriptorHeap::OnFrameStart();
	globalFrameVersioning->OnFrameStart();
	const auto rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const auto rtvHeapStart = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	const auto postprocessDescriptorTable = GPUDescriptorHeap::FillPostprocessGPUDescriptorTableStore(postprocessCPUDescriptorHeap);
	viewport->Render(output.Get(), rendertarget.Get(), ZBuffer.Get(), HDRSurfaces, LDRSurface.Get(),
		COCBuffer.Get(), dilatedCOCBuffer.Get(), halfresDOFSurface.Get(), DOFLayers.Get(), lensFlareSurface.Get(), bloomUpChain.Get(), bloomDownChain.Get(), luminanceReductionBuffer.Get(),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, SCENE_RTV, rtvSize), dsvHeap->GetCPUDescriptorHandleForHeapStart(),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, DOF_LAYERS_RTVs, rtvSize), CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, LENS_FLARE_RTV, rtvSize),
		postprocessDescriptorTable, postprocessCPUDescriptorHeap, width, height);
	CheckHR(swapChain->Present(vsync, 0));
	globalFrameVersioning->OnFrameFinish();
	viewport->OnFrameFinish();
	Impl::CmdListPool::OnFrameFinish();
	OnFrameFinish();
}

void RenderOutput::CreateOffscreenSurfaces(UINT width, UINT height)
{
	// cleanup all at once beforehand in order to reduce chances of VRAM fragmentation
	rendertarget.Reset();
	ZBuffer.Reset();
	HDRSurfaces[0].Reset();
	HDRSurfaces[1].Reset();
	COCBuffer.Reset();
	dilatedCOCBuffer.Reset();
	halfresDOFSurface.Reset();
	DOFLayers.Reset();
	LDRSurface.Reset();
	bloomUpChain.Reset();
	bloomDownChain.Reset();

	const auto rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const auto rtvHeapStart = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	const DXGI_SAMPLE_DESC MSAA_mode = Config::MSAA();

	// create MSAA rendertarget
	extern const float backgroundColor[4];
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, width, height, 1, 1, MSAA_mode.Count, MSAA_mode.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&CD3DX12_CLEAR_VALUE(Config::HDRFormat, backgroundColor),
		IID_PPV_ARGS(rendertarget.GetAddressOf())
	));

	// fill scene RTV heap
	device->CreateRenderTargetView(rendertarget.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, SCENE_RTV, rtvSize));

	// create z/stencil buffer
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::ZFormat::ROP, width, height, 1, 1, MSAA_mode.Count, MSAA_mode.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&CD3DX12_CLEAR_VALUE(Config::ZFormat::ROP, 1.f, 0xef),
		IID_PPV_ARGS(ZBuffer.GetAddressOf())
	));

	// fill DSV heap
	device->CreateDepthStencilView(ZBuffer.Get(), NULL, dsvHeap->GetCPUDescriptorHandleForHeapStart());

	// create HDR offscreen surfaces
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, width, height, 1, 1),
		D3D12_RESOURCE_STATE_RESOLVE_DEST,
		NULL,
		IID_PPV_ARGS(HDRSurfaces[0].GetAddressOf())
	));
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		NULL,
		IID_PPV_ARGS(HDRSurfaces[1].GetAddressOf())
	));

	// create LDR offscreen surface (D3D12 disallows UAV on swap chain backbuffers)
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::DisplayFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		NULL,
		IID_PPV_ARGS(LDRSurface.GetAddressOf())
	));

	// create fullres CoC buffer
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		NULL,
		IID_PPV_ARGS(COCBuffer.GetAddressOf())
	));

	// create halfres dilated CoC buffer
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16_FLOAT, (width + 1) / 2, (height + 1) / 2, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		NULL,
		IID_PPV_ARGS(dilatedCOCBuffer.GetAddressOf())
	));

	// create halfres DOF color surface
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, (width + 1) / 2, (height + 1) / 2, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		NULL,
		IID_PPV_ARGS(halfresDOFSurface.GetAddressOf())
	));

	// create DOF blur layers
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::DOFLayersFormat, (width + 1) / 2, (height + 1) / 2, 4, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		NULL,
		IID_PPV_ARGS(DOFLayers.GetAddressOf())
	));

	// fill DOF RTV heap
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc
		{
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY,
			.Texture2DArray
			{
				.MipSlice = 0,
				.FirstArraySlice = 0,
				.ArraySize = 2,
				.PlaneSlice = 0
			}
		};

		// near layers
		device->CreateRenderTargetView(DOFLayers.Get(), &rtvDesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, DOF_NEAR_RTV, rtvSize));

		// far layers
		rtvDesc.Texture2DArray.FirstArraySlice = 2;
		device->CreateRenderTargetView(DOFLayers.Get(), &rtvDesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, DOF_FAR_RTV, rtvSize));
	}

	// create lens flare surface
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, (width + 1) / 2, (height + 1) / 2, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		NULL,
		IID_PPV_ARGS(lensFlareSurface.GetAddressOf())
	));

	// fill lens flare RTV heap
	device->CreateRenderTargetView(lensFlareSurface.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, LENS_FLARE_RTV, rtvSize));

	// create bloom chains
	{
		unsigned long bloomUpChainLen;
		// TODO: replace with C++20 std::log2p1 with halfres
		_BitScanReverse(&bloomUpChainLen, max(width, height));
		bloomUpChainLen = min(bloomUpChainLen, 6ul);
		const auto bloomDownChainLen = bloomUpChainLen - 1;

		// up
		CheckHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, width / 2, height / 2, 1, bloomUpChainLen, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(bloomUpChain.GetAddressOf())
		));

		// down
		CheckHR(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, width / 2, height / 2, 1, bloomDownChainLen, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(bloomDownChain.GetAddressOf())
		));
	}

	// fill postprocess descriptors CPU backing store
	const auto luminanceReductionTexDispatchSize = CSConfig::LuminanceReduction::TexturePass::DispatchSize({ width, height });
	postprocessCPUDescriptorHeap.Fill(ZBuffer.Get(), HDRSurfaces[0].Get(), HDRSurfaces[1].Get(), LDRSurface.Get(), COCBuffer.Get(), dilatedCOCBuffer.Get(), halfresDOFSurface.Get(), DOFLayers.Get(),
		lensFlareSurface.Get(), bloomUpChain.Get(), bloomDownChain.Get(), luminanceReductionBuffer.Get(), luminanceReductionTexDispatchSize.x * luminanceReductionTexDispatchSize.y);
}