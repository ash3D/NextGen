/**
\author		Alexey Shaydurov aka ASH
\date		15.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "GPU descriptor heap.h"
#include "tonemap resource views stage.h"
#include "frame versioning.h"

using namespace Renderer::Impl;
using namespace Descriptors;
using Microsoft::WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;

ComPtr<ID3D12DescriptorHeap> GPUDescriptorHeap::Impl::CreateHeap()
{
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

	const D3D12_DESCRIPTOR_HEAP_DESC desc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,					// type
		TonemapResourceViewsStage::ViewCount * maxFrameLatency,	// count
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE				// GPU visible
	};
	ComPtr<ID3D12DescriptorHeap> result;
	CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(result.GetAddressOf())));
	NameObjectF(result.Get(), L"GPU descriptor heap (heap start CPU address: %p)", result->GetCPUDescriptorHandleForHeapStart());
	return result;
}

D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHeap::SetCurFrameTonemapReductionDescs(const TonemapResourceViewsStage &stage, UINT backBufferIdx)
{
	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const auto GPUHeapOffset = globalFrameVersioning->GetContinuousRingIdx() * TonemapResourceViewsStage::ViewCount * descriptorSize;
	const CD3DX12_CPU_DESCRIPTOR_HANDLE dst(GetHeap()->GetCPUDescriptorHandleForHeapStart(), GPUHeapOffset), src(stage.allocation->GetCPUDescriptorHandleForHeapStart());
	device->CopyDescriptorsSimple(TonemapResourceViewsStage::ViewCount, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(GetHeap()->GetGPUDescriptorHandleForHeapStart(), GPUHeapOffset);
}