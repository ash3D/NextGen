/**
\author		Alexey Shaydurov aka ASH
\date		25.10.2018 (c)Korotkov Andrey

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

void TonemapResourceViewsStage::Fill(ID3D12Resource *src, ID3D12Resource *dst, ID3D12Resource *reductionBuffer, UINT reductionBufferLength)
{
	reductionBufferLength *= 2;	// to account for {avg, max} layout, RAW buffer view specify num of 32bit elements

	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const auto heapStart = allocation->GetCPUDescriptorHandleForHeapStart();

	// SrcSRV
	device->CreateShaderResourceView(src, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, SrcSRV, descriptorSize));

	// DstUAV
	device->CreateUnorderedAccessView(dst, NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DstUAV, descriptorSize));

	// ReductionBufferUAV
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVdesc =
		{
			DXGI_FORMAT_R32_TYPELESS,	// RAW buffer
			D3D12_UAV_DIMENSION_BUFFER
		};

		UAVdesc.Buffer =
		{
			0,							// FirstElement
			reductionBufferLength,		// NumElements
			0,							// StructureByteStride
			0,							// CounterOffsetInBytes
			D3D12_BUFFER_UAV_FLAG_RAW
		};
		
		device->CreateUnorderedAccessView(reductionBuffer, NULL, &UAVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, ReductionBufferUAV, descriptorSize));
	}

	// ReductionBufferSRV
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVdesc =
		{
			DXGI_FORMAT_R32_TYPELESS,	// RAW buffer
			D3D12_SRV_DIMENSION_BUFFER,
			D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
		};

		SRVdesc.Buffer =
		{
			0,							// FirstElement
			reductionBufferLength,		// NumElements
			0,							// StructureByteStride
			D3D12_BUFFER_SRV_FLAG_RAW
		};

		device->CreateShaderResourceView(reductionBuffer, &SRVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, ReductionBufferSRV, descriptorSize));
	}
}