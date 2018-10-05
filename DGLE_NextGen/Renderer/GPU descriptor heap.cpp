/**
\author		Alexey Shaydurov aka ASH
\date		05.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "GPU descriptor heap.h"
#include "frame versioning.h"

using namespace Renderer::Impl::Descriptors;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;

ComPtr<ID3D12DescriptorHeap> GPUDescriptorHeap::CreateHeap()
{
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

	const D3D12_DESCRIPTOR_HEAP_DESC desc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,							// type
		TonemapResourceViewsStage::ViewCount * Impl::maxFrameLatency,	// count
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE						// GPU visible
	};
	ComPtr<ID3D12DescriptorHeap> result;
	CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObjectF(result.Get(), L"GPU descriptor heap (heap start CPU address: %p)", result->GetCPUDescriptorHandleForHeapStart());
	return result;
}

D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHeap::SetCurFrameTonemapReductionDescs(const TonemapResourceViewsStage &stage)
{
	const auto offset = Impl::globalFrameVersioning->GetContinuousRingIdx() * TonemapResourceViewsStage::ViewCount * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const CD3DX12_CPU_DESCRIPTOR_HANDLE src(stage.allocation->GetCPUDescriptorHandleForHeapStart()), dst(heap->GetCPUDescriptorHandleForHeapStart(), offset);
	device->CopyDescriptorsSimple(TonemapResourceViewsStage::ViewCount, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(heap->GetGPUDescriptorHandleForHeapStart(), offset);
}