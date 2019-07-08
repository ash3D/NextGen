#include "stdafx.h"
#include "GPU descriptor heap.h"
#include "tonemap resource views stage.h"
#include "frame versioning.h"
#include "tracked resource.inl"

// Kepler driver issue workaround, it fails to create heap after a lot of textures have been created
#define ENABLE_PREALLOCATION 1

using namespace std;
using namespace Renderer::Impl;
using namespace Descriptors;

extern Microsoft::WRL::ComPtr<ID3D12Device2> device;
static constexpr auto heapStaticBlockSize = TonemapResourceViewsStage::ViewCount * maxFrameLatency;
static UINT heapSize = heapStaticBlockSize;
decltype(GPUDescriptorHeap::AllocationClient::registeredClients) GPUDescriptorHeap::AllocationClient::registeredClients;
#if ENABLE_PREALLOCATION
static constexpr UINT preallocSize = 16384U;
static bool commited;
#endif

static TrackedResource<ID3D12DescriptorHeap> CreateHeap(UINT size)
{
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;
	static unsigned long version;

	TrackedResource<ID3D12DescriptorHeap> heap;
	const D3D12_DESCRIPTOR_HEAP_DESC desc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,		// type
		size,										// count
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE	// GPU visible
	};
	CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.GetAddressOf())));
	NameObjectF(heap.Get(), L"GPU descriptor heap [%lu] (heap start CPU address: %p)", version++, heap->GetCPUDescriptorHandleForHeapStart());
	return move(heap);
}

TrackedResource<ID3D12DescriptorHeap> GPUDescriptorHeap::Impl::PreallocateHeap()
{
#if ENABLE_PREALLOCATION
	return CreateHeap(preallocSize);
#else
	return nullptr;
#endif
}

using GPUDescriptorHeap::Impl::heap;

void GPUDescriptorHeap::OnFrameStart()
{
#if ENABLE_PREALLOCATION
	if (!commited)
#else
	if (!heap)
#endif
	{
#if ENABLE_PREALLOCATION
		if (heap->GetDesc().NumDescriptors < heapSize || globalFrameVersioning->GetCurFrameID() < globalFrameVersioning->GetCompletedFrameID())
#endif
		{
			// create heap
			heap = CreateHeap(heapSize);
		}

		// go over registered clients and invoke allocation handler\
		TODO: use C++20 initializer in range-based for
		const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE CPU_ptr(heap->GetCPUDescriptorHandleForHeapStart(), heapStaticBlockSize, descriptorSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GPU_ptr(heap->GetGPUDescriptorHandleForHeapStart(), heapStaticBlockSize, descriptorSize);
		for (const auto &allocClient : AllocationClient::registeredClients)
		{
			allocClient.first->GPUDescriptorsAllocation = GPU_ptr.ptr;
			allocClient.first->Commit(CPU_ptr);
			CPU_ptr.Offset(allocClient.second, descriptorSize);
			GPU_ptr.Offset(allocClient.second, descriptorSize);
		}

#if ENABLE_PREALLOCATION
		commited = true;
#endif
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHeap::SetCurFrameTonemapReductionDescs(const TonemapResourceViewsStage &stage, UINT backBufferIdx)
{
	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const auto GPUHeapOffset = globalFrameVersioning->GetContinuousRingIdx() * TonemapResourceViewsStage::ViewCount * descriptorSize;
	const CD3DX12_CPU_DESCRIPTOR_HANDLE dst(GetHeap()->GetCPUDescriptorHandleForHeapStart(), GPUHeapOffset), src(stage.allocation->GetCPUDescriptorHandleForHeapStart());
	device->CopyDescriptorsSimple(TonemapResourceViewsStage::ViewCount, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(GetHeap()->GetGPUDescriptorHandleForHeapStart(), GPUHeapOffset);
}

GPUDescriptorHeap::AllocationClient::AllocationClient(unsigned allocSize)
{
	// force heap to recreate on next frame
#if ENABLE_PREALLOCATION
	commited = false;
#else
	heap.Reset();
#endif

	registeredClients.emplace_back(this, allocSize);
	heapSize += allocSize;
	clientLocation = prev(registeredClients.cend());
}

// heap recreation is not mandatory, just descriptor memory would not be reclaimed and would remain wasted (trade it for perf here)
GPUDescriptorHeap::AllocationClient::~AllocationClient()
{
	heapSize -= clientLocation->second;
	registeredClients.erase(clientLocation);
}