#include "stdafx.h"
#include "frame versioning.h"
#include "occlusion query batch.h"
#include "DMA engine.h"
#include "render output.hh"	// for tonemap reduction buffer
#include "viewport.hh"		// for tonemap root sig & PSOs
#include "world.hh"
#include "terrain.hh"
#include "terrain materials.hh"
#include "object 3D.hh"
#include "tracked resource.inl"
#include "GPU stream buffer allocator.inl"

// auto init does not work with dll, hangs with Graphics Debugging
#define ENABLE_AUTO_INIT	0
#define ENABLE_GBV			0

using namespace std;
using Renderer::RenderOutput;
using Renderer::Impl::globalFrameVersioning;
using Renderer::Impl::Viewport;
using Renderer::Impl::World;
using Renderer::Impl::TerrainVectorLayer;
using Renderer::Impl::Object3D;
using Microsoft::WRL::ComPtr;
namespace TerrainMaterials = Renderer::TerrainMaterials;

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

	// validate HLSL SIMD feature support
	D3D12_FEATURE_DATA_D3D12_OPTIONS1 caps;
	CheckHR(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &caps, sizeof caps));
	if (!caps.WaveOps)
		throw runtime_error("Old GPU or driver: HLSL SIMD ops not supported.");

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

static auto CreateGraphicsCommandQueue()
{
	extern ComPtr<ID3D12Device2> device;

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

static auto CreateDMACommandQueue()
{
	extern ComPtr<ID3D12Device2> device;

	const D3D12_COMMAND_QUEUE_DESC desc =
	{
		D3D12_COMMAND_LIST_TYPE_COPY,
		D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		D3D12_COMMAND_QUEUE_FLAG_NONE
	};
	ComPtr<ID3D12CommandQueue> cmdQueue;
	CheckHR(device->CreateCommandQueue(&desc, IID_PPV_ARGS(cmdQueue.GetAddressOf())));
	NameObject(cmdQueue.Get(), L"DMA engine command queue");
	return cmdQueue;
}

static void PrintError(const exception_ptr &error, const char object[])
{
	try
	{
		rethrow_exception(error);
	}
	catch (HRESULT hr)
	{
		clog << "Fail to automatically create " << object << ", manual call to 'InitRenderer()' required (hr=" << hr << ")." << endl;
	}
	catch (const exception &error)
	{
		clog << "Fail to automatically create " << object << ": " << error.what() << ". Manual call to 'InitRenderer()' required." << endl;
	}
	catch (...)
	{
		clog << "Fail to automatically create " << object << " (unknown error). Manual call to 'InitRenderer()' required." << endl;
	}
}

static inline ComPtr<IDXGIFactory5> TryCreateFactory()
{
#if ENABLE_AUTO_INIT
	try
	{
		return CreateFactory();
	}
	catch (...)
	{
		PrintError(current_exception(), "DXGI factory");
	}
#endif
	return nullptr;
}

static inline ComPtr<ID3D12Device2> TryCreateDevice()
{
#if ENABLE_AUTO_INIT
	try
	{
		return CreateDevice();
	}
	catch (...)
	{
		PrintError(current_exception(), "D3D12 device");
	}
#endif
	return nullptr;
}

template<typename Result>
static Result Try(Result Create(), const char object[])
{
	extern ComPtr<ID3D12Device2> device;
	if (device)
	{
		try
		{
			return Create();
		}
		catch (...)
		{
			PrintError(current_exception(), object);
		}
	}
	device.Reset();	// force recreation everything in 'InitRenderer()'
	return {};
}

ComPtr<IDXGIFactory5> factory = TryCreateFactory();
ComPtr<ID3D12Device2> device = TryCreateDevice();
ComPtr<ID3D12CommandQueue> gfxQueue = Try(CreateGraphicsCommandQueue, "main GFX command queue"), dmaQueue = Try(CreateDMACommandQueue, "DMA engine command queue");

template<class Optional>
static inline Optional TryCreate(const char object[])
{
	if (device)
	{
		try
		{
			return Optional(in_place);
		}
		catch (...)
		{
			PrintError(current_exception(), object);
		}
	}
	return nullopt;
}

namespace Renderer::Impl::Descriptors::TextureSampers::Impl
{
	ComPtr<ID3D12DescriptorHeap> CreateHeap(), heap = Try(CreateHeap, "GPU texture sampler heap");
}

ComPtr<ID3D12Resource> RenderOutput::tonemapReductionBuffer = device ? RenderOutput::CreateTonemapReductionBuffer() : nullptr;

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
	if (globalFrameVersioning->GetCurFrameID() > globalFrameVersioning->GetCompletedFrameID())
	{
		lock_guard<decltype(mtx)> lck(mtx);
		retiredResources.push({ globalFrameVersioning->GetCurFrameID(), move(resource) });
	}
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
	Viewport::tonemapRootSig						= Try(Viewport::CreateTonemapRootSig, "tonemapping root signature"),
	TerrainVectorLayer::cullPassRootSig				= Try(TerrainVectorLayer::CreateCullPassRootSig, "terrain occlusion query root signature"),
	TerrainVectorLayer::AABB_rootSig				= Try(TerrainVectorLayer::CreateAABB_RootSig, "terrain AABB visualization root signature"),
	TerrainMaterials::Flat::rootSig					= Try(TerrainMaterials::Flat::CreateRootSig, "terrain flat material root signature"),
	TerrainMaterials::Textured::rootSig				= Try(TerrainMaterials::Textured::CreateRootSig, "terrain textured material root signature"),
	TerrainMaterials::Standard::rootSig				= Try(TerrainMaterials::Standard::CreateRootSig, "terrain standard material root signature"),
	TerrainMaterials::Extended::rootSig				= Try(TerrainMaterials::Extended::CreateRootSig, "terrain extended material root signature"),
	World::xformAABB_rootSig						= Try(World::CreateXformAABB_RootSig, "Xform 3D AABB root signature"),
	World::cullPassRootSig							= Try(World::CreateCullPassRootSig, "world objects occlusion query root signature"),
	World::AABB_rootSig								= Try(World::CreateAABB_RootSig, "world 3D objects AABB visualization root signature"),
	Object3D::rootSig								= Try(Object3D::CreateRootSig, "object 3D root signature");
ComPtr<ID3D12PipelineState>
	Viewport::tonemapTextureReductionPSO			= Try(Viewport::CreateTonemapTextureReductionPSO, "tonemap texture reduction PSO"),
	Viewport::tonemapBufferReductionPSO				= Try(Viewport::CreateTonemapBufferReductionPSO, "tonemap buffer reduction PSO"),
	Viewport::tonemapPSO							= Try(Viewport::CreateTonemapPSO, "tonemapping PSO"),
	TerrainVectorLayer::cullPassPSO					= Try(TerrainVectorLayer::CreateCullPassPSO, "terrain occlusion query PSO"),
	TerrainVectorLayer::AABB_PSO					= Try(TerrainVectorLayer::CreateAABB_PSO, "terrain AABB visualization PSO"),
	TerrainMaterials::Flat::PSO						= Try(TerrainMaterials::Flat::CreatePSO, "terrain flat material PSO"),
	TerrainMaterials::Textured::PSO					= Try(TerrainMaterials::Textured::CreatePSO, "terrain textured material PSO"),
	TerrainMaterials::Standard::PSO					= Try(TerrainMaterials::Standard::CreatePSO, "terrain standard material PSO"),
	TerrainMaterials::Extended::PSO					= Try(TerrainMaterials::Extended::CreatePSO, "terrain extended material PSO"),
	World::xformAABB_PSO							= Try(World::CreateXformAABB_PSO, "Xform 3D AABB PSO");
decltype(World::cullPassPSOs) World::cullPassPSOs	= Try(World::CreateCullPassPSOs, "world objects occlusion query passes PSOs");
decltype(World::AABB_PSOs) World::AABB_PSOs			= Try(World::CreateAABB_PSOs, "world 3D objects AABB visualization PSOs");
decltype(Object3D::PSOs) Object3D::PSOs				= Try(Object3D::CreatePSOs, "object 3D PSOs");

// should be defined before globalFrameVersioning in order to be destroyed after waiting in globalFrameVersioning dtor completes
using Renderer::Impl::World;
ComPtr<ID3D12Resource> World::globalGPUBuffer = Try(World::CreateGlobalGPUBuffer, "global GPU buffer");
#if PERSISTENT_MAPS
volatile World::PerFrameData *World::globalGPUBuffer_CPU_ptr = World::TryMapGlobalGPUBuffer();
// define here to enable inline
inline volatile World::PerFrameData *World::TryMapGlobalGPUBuffer()
{
	return globalGPUBuffer ? MapGlobalGPUBuffer(&CD3DX12_RANGE(0, 0)) : nullptr;
}
#endif

namespace Renderer::Impl
{
	decltype(globalFrameVersioning) globalFrameVersioning = TryCreate<decltype(globalFrameVersioning)>("global frame versionong");
}

// tracked resource should be destroyed before globalFrameVersioning => should be defined after globalFrameVersioning
namespace Renderer::Impl::Descriptors::GPUDescriptorHeap::Impl
{
	TrackedResource<ID3D12DescriptorHeap> heap;
}
namespace OcclusionCulling = Renderer::Impl::OcclusionCulling;
using OcclusionCulling::QueryBatchBase;
using OcclusionCulling::QueryBatch;
decltype(QueryBatchBase::heapPool) QueryBatchBase::heapPool;
decltype(QueryBatch<OcclusionCulling::TRANSIENT>::resultsPool) QueryBatch<OcclusionCulling::TRANSIENT>::resultsPool;

// allocators contains tracked resource (=> after globalFrameVersioning)
decltype(TerrainVectorLayer::GPU_AABB_allocator) TerrainVectorLayer::GPU_AABB_allocator = TryCreate<decltype(TerrainVectorLayer::GPU_AABB_allocator)>("GPU AABB allocator for terrain vector layers");
decltype(World::GPU_AABB_allocator) World::GPU_AABB_allocator = TryCreate<decltype(World::GPU_AABB_allocator)>("GPU AABB allocator for world 3D objects");
decltype(World::xformedAABBsStorage) World::xformedAABBsStorage;

namespace Renderer::DMA::Impl
{
	decltype(cmdBuffers) cmdBuffers = Try(CreateCmdBuffers, "DMA engine command buffers");
	ComPtr<ID3D12Fence> fence = Try(CreateFence, "DMA engine fence");
}

extern bool enableDebugDraw = false;

extern void __cdecl InitRenderer()
{
	if (!factory || !device)
	{
		namespace TextureSampers = Renderer::Impl::Descriptors::TextureSampers;
		namespace DMAEngine = Renderer::DMA::Impl;
		factory									= CreateFactory();
		device									= CreateDevice();
		gfxQueue								= CreateGraphicsCommandQueue();
		dmaQueue								= CreateDMACommandQueue();
		TextureSampers::Impl::heap				= TextureSampers::Impl::CreateHeap();
		RenderOutput::tonemapReductionBuffer	= RenderOutput::CreateTonemapReductionBuffer();
		Viewport::tonemapRootSig				= Viewport::CreateTonemapRootSig();
		Viewport::tonemapTextureReductionPSO	= Viewport::CreateTonemapTextureReductionPSO();
		Viewport::tonemapBufferReductionPSO		= Viewport::CreateTonemapBufferReductionPSO();
		Viewport::tonemapPSO					= Viewport::CreateTonemapPSO();
		TerrainVectorLayer::cullPassRootSig		= TerrainVectorLayer::CreateCullPassRootSig();
		TerrainVectorLayer::AABB_rootSig		= TerrainVectorLayer::CreateAABB_RootSig();
		TerrainVectorLayer::cullPassPSO			= TerrainVectorLayer::CreateCullPassPSO();
		TerrainVectorLayer::AABB_PSO			= TerrainVectorLayer::CreateAABB_PSO();
		TerrainMaterials::Flat::rootSig			= TerrainMaterials::Flat::CreateRootSig();
		TerrainMaterials::Textured::rootSig		= TerrainMaterials::Textured::CreateRootSig();
		TerrainMaterials::Standard::rootSig		= TerrainMaterials::Standard::CreateRootSig();
		TerrainMaterials::Extended::rootSig		= TerrainMaterials::Extended::CreateRootSig();
		TerrainMaterials::Flat::PSO				= TerrainMaterials::Flat::CreatePSO();
		TerrainMaterials::Textured::PSO			= TerrainMaterials::Textured::CreatePSO();
		TerrainMaterials::Standard::PSO			= TerrainMaterials::Standard::CreatePSO();
		TerrainMaterials::Extended::PSO			= TerrainMaterials::Extended::CreatePSO();
		World::xformAABB_rootSig				= World::CreateXformAABB_RootSig();
		World::cullPassRootSig					= World::CreateCullPassRootSig();
		World::AABB_rootSig						= World::CreateAABB_RootSig();
		World::xformAABB_PSO					= World::CreateXformAABB_PSO();
		World::cullPassPSOs						= World::CreateCullPassPSOs();
		World::AABB_PSOs						= World::CreateAABB_PSOs();
		Object3D::rootSig						= Object3D::CreateRootSig();
		Object3D::PSOs							= Object3D::CreatePSOs();
		World::globalGPUBuffer					= World::CreateGlobalGPUBuffer();
#if PERSISTENT_MAPS
		World::globalGPUBuffer_CPU_ptr			= World::MapGlobalGPUBuffer();
#endif
		globalFrameVersioning.emplace();
		TerrainVectorLayer::GPU_AABB_allocator.emplace();
		World::GPU_AABB_allocator.emplace();
		DMAEngine::cmdBuffers					= DMAEngine::CreateCmdBuffers();
		DMAEngine::fence						= DMAEngine::CreateFence();
	}
}