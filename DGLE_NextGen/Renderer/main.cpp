/**
\author		Alexey Shaydurov aka ASH
\date		17.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "frame versioning.h"
#include "occlusion query batch.h"
#include "world.hh"
#include "terrain.hh"
#include "tracked resource.inl"
#include "GPU stream buffer allocator.inl"

// auto init does not work with dll, hangs with Graphics Debugging
#define ENABLE_AUTO_INIT 0

using namespace std;
using Renderer::Impl::globalFrameVersioning;
using Renderer::TerrainVectorLayer;
using Microsoft::WRL::ComPtr;

static constexpr size_t maxD3D12NameLength = 256;

void NameObject(ID3D12Object *object, LPCWSTR name) noexcept
{
	if (const HRESULT hr = object->SetName(name); FAILED(hr))
	{
		if (fwide(stderr, 0) < 0)
		{
			[[maybe_unused]] const auto reopened = freopen(NULL, "w", stderr);
			assert(reopened);
		}

		wcerr << "Fail to set name " << quoted(name) << " for D3D12 object \'" << object << "\' (hr=" << hr << ")." << endl;

		// reopen again to reset orientation
		[[maybe_unused]] const auto reopened = freopen(NULL, "w", stderr);
		assert(reopened);
	}
}

void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept
{
	WCHAR buf[maxD3D12NameLength];
	va_list args;
	va_start(args, format);
	const int length = vswprintf(buf, size(buf), format, args);
	if (length < 0)
		perror("Fail to compose name for D3D12 object");
	else
		NameObject(object, buf);
	assert(length >= 0 && length < maxD3D12NameLength);
	va_end(args);
}

static auto CreateFactory()
{
	UINT creationFlags = 0;

#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	const HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()));
	if (SUCCEEDED(hr))
	{
		debugController->EnableDebugLayer();
		creationFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
	else
		cerr << "Fail to enable D3D12 debug layer (hr=" << hr << ")." << endl;
#endif

	ComPtr<IDXGIFactory5> factory;
	CheckHR(CreateDXGIFactory2(creationFlags, IID_PPV_ARGS(factory.GetAddressOf())));
	return factory;
}

static auto CreateDevice()
{
	ComPtr<ID3D12Device2> device;
	CheckHR(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf())));
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
	CheckHR(device->CreateCommandQueue(&desc, IID_PPV_ARGS(cmdQueue.GetAddressOf())));
	NameObject(cmdQueue.Get(), L"main GFX command queue");
	return cmdQueue;
}

static inline ComPtr<IDXGIFactory5> TryCreateFactory()
{
#if ENABLE_AUTO_INIT
	try
	{
		return CreateFactory();
	}
	catch (HRESULT hr)
	{
		clog << "Fail to automatically create DXGI factory, manual call to 'InitRenderer()' required (hr=" << hr << ")." << endl;
		return nullptr;
	}
#else
	return nullptr;
#endif
}

static inline ComPtr<ID3D12Device2> TryCreateDevice()
{
#if ENABLE_AUTO_INIT
	try
	{
		return CreateDevice();
	}
	catch (HRESULT hr)
	{
		clog << "Fail to automatically create D3D12 device, manual call to 'InitRenderer()' required (hr=" << hr << ")." << endl;
		return nullptr;
	}
#else
	return nullptr;
#endif
}

ComPtr<IDXGIFactory5> factory = TryCreateFactory();
ComPtr<ID3D12Device2> device = TryCreateDevice();
ComPtr<ID3D12CommandQueue> cmdQueue = device ? CreateCommandQueue() : nullptr;

struct RetiredResource
{
	UINT64 frameID;
	ComPtr<IUnknown> resource;
};
static queue<RetiredResource> retiredResources;

// keeps resource alive while accessed by GPU
void RetireResource(ComPtr<IUnknown> resource)
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

#pragma region root sigs & PSOs
ComPtr<ID3D12RootSignature>
	TerrainVectorLayer::CRenderStage::COcclusionQueryPass	::rootSig	= TerrainVectorLayer::CRenderStage::COcclusionQueryPass	::TryCreateRootSig(),
	TerrainVectorLayer::CRenderStage::CMainPass				::rootSig	= TerrainVectorLayer::CRenderStage::CMainPass			::TryCreateRootSig();
ComPtr<ID3D12PipelineState>
	TerrainVectorLayer::CRenderStage::COcclusionQueryPass	::PSO		= TerrainVectorLayer::CRenderStage::COcclusionQueryPass	::TryCreatePSO(),
	TerrainVectorLayer::CRenderStage::CMainPass				::PSO		= TerrainVectorLayer::CRenderStage::CMainPass			::TryCreatePSO();

#pragma region TryCreate...()
inline ComPtr<ID3D12RootSignature> TerrainVectorLayer::CRenderStage::COcclusionQueryPass::TryCreateRootSig()
{
	return device ? CreateRootSig() : nullptr;
}

inline ComPtr<ID3D12RootSignature> TerrainVectorLayer::CRenderStage::CMainPass::TryCreateRootSig()
{
	return device ? CreateRootSig() : nullptr;
}

inline ComPtr<ID3D12PipelineState> TerrainVectorLayer::CRenderStage::COcclusionQueryPass::TryCreatePSO()
{
	return device ? CreatePSO() : nullptr;
}

inline ComPtr<ID3D12PipelineState> TerrainVectorLayer::CRenderStage::CMainPass::TryCreatePSO()
{
	return device ? CreatePSO() : nullptr;
}
#pragma endregion define here to enable inline
#pragma endregion

// should be defined before globalFrameVersioning in order to be destroyed after waiting in globalFrameVersioning dtor completes
using Renderer::Impl::World;
ComPtr<ID3D12Resource> World::perFrameCB = World::TryCreatePerFrameCB();
// define Try...() functions here to enable inline
inline ComPtr<ID3D12Resource> World::TryCreatePerFrameCB()
{
	return device ? CreatePerFrameCB() : nullptr;
}
#if PERSISTENT_MAPS
volatile World::PerFrameData *World::perFrameCB_CPU_ptr = World::TryMapPerFrameCB();
inline volatile World::PerFrameData *World::TryMapPerFrameCB()
{
	return perFrameCB ? MapPerFrameCB(&CD3DX12_RANGE(0, 0)) : nullptr;
}
#endif

namespace Renderer::Impl
{
#if defined _MSC_VER && _MSC_VER <= 1912
	decltype(globalFrameVersioning) globalFrameVersioning;
#else
	// guaranteed copy elision required
	decltype(globalFrameVersioning) globalFrameVersioning(device ? decltype(globalFrameVersioning)(in_place) : nullopt);
#endif
}

// tracked resource should be destroyed before globalFrameVersioning => should be defined after globalFrameVersioning
using Renderer::Impl::OcclusionCulling::QueryBatch;
decltype(QueryBatch::heapPool) QueryBatch::heapPool;
decltype(QueryBatch::resultsPool) QueryBatch::resultsPool;

// allocator contains tracked resource
#if defined _MSC_VER && _MSC_VER <= 1912
decltype(TerrainVectorLayer::GPU_AABB_allocator) TerrainVectorLayer::GPU_AABB_allocator;
#else
// guaranteed copy elision required
decltype(TerrainVectorLayer::GPU_AABB_allocator) TerrainVectorLayer::GPU_AABB_allocator(device ? decltype(TerrainVectorLayer::GPU_AABB_allocator)(in_place) : nullopt);
#endif

extern void __cdecl InitRenderer()
{
	if (!factory)
		factory = CreateFactory();

	if (!device)
	{
		device = CreateDevice();
		cmdQueue = CreateCommandQueue();
		TerrainVectorLayer::CRenderStage::COcclusionQueryPass	::rootSig	= TerrainVectorLayer::CRenderStage::COcclusionQueryPass	::CreateRootSig();
		TerrainVectorLayer::CRenderStage::CMainPass				::rootSig	= TerrainVectorLayer::CRenderStage::CMainPass			::CreateRootSig();
		TerrainVectorLayer::CRenderStage::COcclusionQueryPass	::PSO		= TerrainVectorLayer::CRenderStage::COcclusionQueryPass	::CreatePSO();
		TerrainVectorLayer::CRenderStage::CMainPass				::PSO		= TerrainVectorLayer::CRenderStage::CMainPass			::CreatePSO();
		World::perFrameCB = World::CreatePerFrameCB();
#if PERSISTENT_MAPS
		World::perFrameCB_CPU_ptr = World::MapPerFrameCB();
#endif
		globalFrameVersioning.emplace();
		TerrainVectorLayer::GPU_AABB_allocator.emplace();
	}
}