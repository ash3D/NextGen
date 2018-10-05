/**
\author		Alexey Shaydurov aka ASH
\date		05.10.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "tonemap resource views stage.h"

using namespace Renderer::Impl::Descriptors;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
	
TonemapResourceViewsStage::TonemapResourceViewsStage()
{
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

	const D3D12_DESCRIPTOR_HEAP_DESC heapDesc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	// type
		ViewCount,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE			// CPU visible
	};
	CheckHR(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(allocation.GetAddressOf())));
	NameObjectF(allocation.Get(), L"CPU descriptor stage for tonemap reduction resources (D3D object: %p, heap start CPU address: %p)", allocation.Get(), allocation->GetCPUDescriptorHandleForHeapStart());
}

void TonemapResourceViewsStage::Fill(ID3D12Resource *texture, ID3D12Resource *reductionBuffer)
{
	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const auto heapStart = allocation->GetCPUDescriptorHandleForHeapStart();

	device->CreateShaderResourceView(texture, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, TextureSRV, descriptorSize));
	device->CreateUnorderedAccessView(reductionBuffer, NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, ReductionBufferUAV, descriptorSize));
}