/**
\author		Alexey Shaydurov aka ASH
\date		05.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "frame versioning.h"
#include "occlusion query batch.h"
#include "world.hh"
#include "terrain.hh"
#include "object 3D.hh"
#include "tracked resource.inl"
#include "GPU stream buffer allocator.inl"

// auto init does not work with dll, hangs with Graphics Debugging
#define ENABLE_AUTO_INIT	0
#define ENABLE_GBV			0

using namespace std;
using Renderer::Impl::globalFrameVersioning;
using Renderer::Impl::World;
using Renderer::Impl::TerrainVectorLayer;
using Renderer::Impl::Object3D;
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

ComPtr<ID3D12RootSignature> CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, LPCWSTR name)
{
	extern ComPtr<ID3D12Device2> device;
	ComPtr<ID3D12RootSignature> result;
	ComPtr<ID3DBlob> sig, error;
	const HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &sig, &error);
	if (error)
	{
		cerr.write((const char *)error->GetBufferPointer(), error->GetBufferSize()) << endl;
	}
	CheckHR(hr);
	CheckHR(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(result.GetAddressOf())));
	NameObject(result.Get(), name);
	return move(result);
}

static auto CreateFactory()
{
	UINT creationFlags = 0;

#ifdef _DEBUG
	ComPtr<ID3D12Debug1> debugController;
	const HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()));
	if (SUCCEEDED(hr))
	{
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(ENABLE_GBV);
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

	// GBV device settings
#if _DEBUG && ENABLE_GBV
	{
		ComPtr<ID3D12DebugDevice1> debugDeviceController;
		if (const HRESULT hr = device.As(&debugDeviceController); FAILED(hr))
			cerr << "Fail to setup device debug settings (hr=" << hr << "). Defaults used.";
		else
		{
			const D3D12_DEBUG_DEVICE_GPU_BASED_VALIDATION_SETTINGS GBVSettings =
			{
				16,
				D3D12_GPU_BASED_VALIDATION_SHADER_PATCH_MODE_NONE,
				D3D12_GPU_BASED_VALIDATION_PIPELINE_STATE_CREATE_FLAG_NONE
			};
			if (const HRESULT hr = debugDeviceController->SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_GPU_BASED_VALIDATION_SETTINGS, &GBVSettings, sizeof GBVSettings); FAILED(hr))
				cerr << "Fail to setup GBV settings (hr=" << hr << ")." << endl;
		}
	}
#endif

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

namespace Renderer::Impl::Descriptors::GPUDescriptorHeap
{
	ComPtr<ID3D12DescriptorHeap> CreateHeap(), heap = device ? CreateHeap() : nullptr;
}

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
	TerrainVectorLayer::cullPassRootSig				= TerrainVectorLayer::TryCreateCullPassRootSig(),
	TerrainVectorLayer::mainPassRootSig				= TerrainVectorLayer::TryCreateMainPassRootSig(),
	World::xformAABB_RootSig						= World::TryCreateXformAABB_RootSig(),
	World::cullPassRootSig							= World::TryCreateCullPassRootSig(),
	World::AABB_RootSig								= World::TryCreateAABB_RootSig(),
	Object3D::rootSig								= Object3D::TryCreateRootSig();
ComPtr<ID3D12PipelineState>
	TerrainVectorLayer::cullPassPSO					= TerrainVectorLayer::TryCreateCullPassPSO(),
	TerrainVectorLayer::mainPassPSO					= TerrainVectorLayer::TryCreateMainPassPSO(),
	TerrainVectorLayer::AABB_PSO					= TerrainVectorLayer::TryCreateAABB_PSO(),
	World::xformAABB_PSO							= World::TryCreateXformAABB_PSO();
decltype(World::cullPassPSOs) World::cullPassPSOs	= World::TryCreateCullPassPSOs();
decltype(World::AABB_PSOs) World::AABB_PSOs			= World::TryCreateAABB_PSOs();
decltype(Object3D::PSOs) Object3D::PSOs				= Object3D::TryCreatePSOs();

#pragma region TryCreate...()
inline ComPtr<ID3D12RootSignature> TerrainVectorLayer::TryCreateCullPassRootSig()
{
	return device ? CreateCullPassRootSig() : nullptr;
}

inline ComPtr<ID3D12RootSignature> TerrainVectorLayer::TryCreateMainPassRootSig()
{
	return device ? CreateMainPassRootSig() : nullptr;
}

inline ComPtr<ID3D12RootSignature> World::TryCreateXformAABB_RootSig()
{
	return device ? CreateXformAABB_RootSig() : nullptr;
}

inline ComPtr<ID3D12RootSignature> World::TryCreateCullPassRootSig()
{
	return device ? CreateCullPassRootSig() : nullptr;
}

inline ComPtr<ID3D12RootSignature> World::TryCreateAABB_RootSig()
{
	return device ? CreateAABB_RootSig() : nullptr;
}

inline ComPtr<ID3D12RootSignature> Object3D::TryCreateRootSig()
{
	return device ? CreateRootSig() : nullptr;
}

inline ComPtr<ID3D12PipelineState> TerrainVectorLayer::TryCreateCullPassPSO()
{
	return device ? CreateCullPassPSO() : nullptr;
}

inline ComPtr<ID3D12PipelineState> TerrainVectorLayer::TryCreateMainPassPSO()
{
	return device ? CreateMainPassPSO() : nullptr;
}

inline ComPtr<ID3D12PipelineState> TerrainVectorLayer::TryCreateAABB_PSO()
{
	return device ? CreateAABB_PSO() : nullptr;
}

inline ComPtr<ID3D12PipelineState> World::TryCreateXformAABB_PSO()
{
	return device ? CreateXformAABB_PSO() : nullptr;
}

inline auto World::TryCreateCullPassPSOs() -> decltype(cullPassPSOs)
{
	return device ? CreateCullPassPSOs() : decltype(cullPassPSOs)();
}

inline auto World::TryCreateAABB_PSOs() -> decltype(AABB_PSOs)
{
	return device ? CreateAABB_PSOs() : decltype(AABB_PSOs)();
}

inline auto Object3D::TryCreatePSOs() -> decltype(PSOs)
{
	return device ? CreatePSOs() : decltype(PSOs)();
}
#pragma endregion define here to enable inline
#pragma endregion

// should be defined before globalFrameVersioning in order to be destroyed after waiting in globalFrameVersioning dtor completes
using Renderer::Impl::World;
ComPtr<ID3D12Resource> World::globalGPUBuffer = World::TryCreateGlobalGPUBuffer();
// define Try...() functions here to enable inline
inline ComPtr<ID3D12Resource> World::TryCreateGlobalGPUBuffer()
{
	return device ? CreateGlobalGPUBuffer() : nullptr;
}
#if PERSISTENT_MAPS
volatile World::PerFrameData *World::globalGPUBuffer_CPU_ptr = World::TryMapGlobalGPUBuffer();
inline volatile World::PerFrameData *World::TryMapGlobalGPUBuffer()
{
	return globalGPUBuffer ? MapGlobalGPUBuffer(&CD3DX12_RANGE(0, 0)) : nullptr;
}
#endif

namespace Renderer::Impl
{
#if defined _MSC_VER && _MSC_VER <= 1915
	decltype(globalFrameVersioning) globalFrameVersioning;
#else
	// guaranteed copy elision required\
	still does not work on MSVC 1913/1914/1915, further investigation reqired
	decltype(globalFrameVersioning) globalFrameVersioning(device ? decltype(globalFrameVersioning)(in_place) : nullopt);
#endif
}

// tracked resource should be destroyed before globalFrameVersioning => should be defined after globalFrameVersioning
namespace OcclusionCulling = Renderer::Impl::OcclusionCulling;
using OcclusionCulling::QueryBatchBase;
using OcclusionCulling::QueryBatch;
decltype(QueryBatchBase::heapPool) QueryBatchBase::heapPool;
decltype(QueryBatch<OcclusionCulling::TRANSIENT>::resultsPool) QueryBatch<OcclusionCulling::TRANSIENT>::resultsPool;

// allocators contains tracked resource
#if defined _MSC_VER && _MSC_VER <= 1915
decltype(TerrainVectorLayer::GPU_AABB_allocator) TerrainVectorLayer::GPU_AABB_allocator;
decltype(World::GPU_AABB_allocator) World::GPU_AABB_allocator;
#else
// guaranteed copy elision required
decltype(TerrainVectorLayer::GPU_AABB_allocator) TerrainVectorLayer::GPU_AABB_allocator(device ? decltype(TerrainVectorLayer::GPU_AABB_allocator)(in_place) : nullopt);
decltype(World::GPU_AABB_allocator) World::GPU_AABB_allocator(device ? decltype(World::GPU_AABB_allocator)(in_place) : nullopt);
#endif
decltype(World::xformedAABBsStorage) World::xformedAABBsStorage;

extern bool enableDebugDraw = false;

extern void __cdecl InitRenderer()
{
	if (!factory)
		factory = CreateFactory();

	if (!device)
	{
		namespace GPUDescriptorHeap = Renderer::Impl::Descriptors::GPUDescriptorHeap;
		device = CreateDevice();
		cmdQueue = CreateCommandQueue();
		GPUDescriptorHeap::heap				= GPUDescriptorHeap::CreateHeap();
		TerrainVectorLayer::cullPassRootSig	= TerrainVectorLayer::CreateCullPassRootSig();
		TerrainVectorLayer::mainPassRootSig	= TerrainVectorLayer::CreateMainPassRootSig();
		TerrainVectorLayer::cullPassPSO		= TerrainVectorLayer::CreateCullPassPSO();
		TerrainVectorLayer::mainPassPSO		= TerrainVectorLayer::CreateMainPassPSO();
		TerrainVectorLayer::AABB_PSO		= TerrainVectorLayer::CreateAABB_PSO();
		World::xformAABB_RootSig			= World::CreateXformAABB_RootSig();
		World::cullPassRootSig				= World::CreateCullPassRootSig();
		World::AABB_RootSig					= World::CreateAABB_RootSig();
		World::xformAABB_PSO				= World::CreateXformAABB_PSO();
		World::cullPassPSOs					= World::CreateCullPassPSOs();
		World::AABB_PSOs					= World::CreateAABB_PSOs();
		Object3D::rootSig					= Object3D::CreateRootSig();
		Object3D::PSOs						= Object3D::CreatePSOs();
		World::globalGPUBuffer				= World::CreateGlobalGPUBuffer();
#if PERSISTENT_MAPS
		World::globalGPUBuffer_CPU_ptr = World::MapGlobalGPUBuffer();
#endif
		globalFrameVersioning.emplace();
		TerrainVectorLayer::GPU_AABB_allocator.emplace();
		World::GPU_AABB_allocator.emplace();
	}
}