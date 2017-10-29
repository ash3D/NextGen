/**
\author		Alexey Shaydurov aka ASH
\date		29.10.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "frame versioning.h"

using namespace std;
using Renderer::Impl::globalFrameVersioning;
using Microsoft::WRL::ComPtr;

static auto CreateFactory()
{
	UINT creationFlags = 0;

#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	const HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
	if (SUCCEEDED(hr))
	{
		debugController->EnableDebugLayer();
		creationFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
	else
		cerr << "Fail to enable D3D12 debug layer (hr=" << hr << ")." << endl;
#endif

	ComPtr<IDXGIFactory5> factory;
	CheckHR(CreateDXGIFactory2(creationFlags, IID_PPV_ARGS(&factory)));
	return factory;
}

static auto CreateDevice()
{
	ComPtr<ID3D12Device2> device;
	CheckHR(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
	return device;
}

static auto CreateCommandQueue()
{
	extern ComPtr<ID3D12Device2> device;
	if (!device)
		throw E_FAIL;

	const D3D12_COMMAND_QUEUE_DESC desc =
	{
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		D3D12_COMMAND_QUEUE_FLAG_NONE
	};
	ComPtr<ID3D12CommandQueue> cmdQueue;
	CheckHR(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&cmdQueue)));
	return cmdQueue;
}

static inline ComPtr<IDXGIFactory5> TryCreateFactory()
{
	return nullptr;
	try
	{
		return CreateFactory();
	}
	catch (HRESULT hr)
	{
		clog << "Fail to automatically create DXGI factory, manual call to 'InitRenderer()' required (hr=" << hr << ")." << endl;
		return nullptr;
	}
}

static inline ComPtr<ID3D12Device2> TryCreateDevice()
{
	return nullptr;
	try
	{
		return CreateDevice();
	}
	catch (HRESULT hr)
	{
		clog << "Fail to automatically create D3D12 device, manual call to 'InitRenderer()' required (hr=" << hr << ")." << endl;
		return nullptr;
	}
}

static inline ComPtr<ID3D12CommandQueue> TryCreateCommandQueue()
{
	return nullptr;
	try
	{
		return CreateCommandQueue();
	}
	catch (HRESULT hr)
	{
		clog << "Fail to automatically create D3D12 command queue, manual call to 'InitRenderer()' required (hr=" << hr << ")." << endl;
		return nullptr;
	}
}

ComPtr<IDXGIFactory5> factory = TryCreateFactory();
ComPtr<ID3D12Device2> device = TryCreateDevice();
ComPtr<ID3D12CommandQueue> cmdQueue = TryCreateCommandQueue();
namespace Renderer::Impl
{
#if defined _MSC_VER && _MSC_VER <= 1911
	decltype(globalFrameVersioning) globalFrameVersioning;
#else
	// guaranteed copy elision required
	decltype(globalFrameVersioning) globalFrameVersioning(device ? decltype(globalFrameVersioning)(in_place) : nullopt);
#endif
}

struct RetiredResource
{
	UINT64 frameID;
	ComPtr<ID3D12Pageable> resource;
};
static queue<RetiredResource> retiredResources;

// keeps resource alive while accessed by GPU
void RetireResource(ComPtr<ID3D12Pageable> resource)
{
	static mutex mtx;
	lock_guard<decltype(mtx)> lck(mtx);
	retiredResources.push({ globalFrameVersioning->GetCurFrameID(), move(resource) });
}

// NOTE: not thread-safe
void OnFrameFinish()
{
	const UINT64 completedFrameID = globalFrameVersioning->GetCompletedFrameID();
	while (!retiredResources.empty() && retiredResources.front().frameID <= completedFrameID)
		retiredResources.pop();
}

extern void __cdecl InitRenderer()
{
	if (!factory)
		factory = CreateFactory();

	if (!device)
	{
		device = CreateDevice();
		cmdQueue = CreateCommandQueue();
		globalFrameVersioning.emplace();
	}
}