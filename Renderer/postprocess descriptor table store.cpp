#include "stdafx.h"
#include "postprocess descriptor table store.h"
#include "config.h"

using namespace Renderer::Impl::Descriptors;
using WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
	
PostprocessDescriptorTableStore::PostprocessDescriptorTableStore()
{
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

	const D3D12_DESCRIPTOR_HEAP_DESC heapDesc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	// type
		TableSize,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE			// CPU visible
	};
	CheckHR(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(allocation.GetAddressOf())));
	NameObjectF(allocation.Get(), L"CPU descriptor stage for postprocess resources (D3D object: %p, heap start CPU address: %p)", allocation.Get(), allocation->GetCPUDescriptorHandleForHeapStart());
}

void PostprocessDescriptorTableStore::Fill(ID3D12Resource *ZBuffer, ID3D12Resource *src, ID3D12Resource *composite, ID3D12Resource *dst,
	ID3D12Resource *COCBuffer, ID3D12Resource *halferDOFSurface, ID3D12Resource *DOFLayers, ID3D12Resource *lensFlareSurface,
	ID3D12Resource *bloomUpChain, ID3D12Resource *bloomDownChain, ID3D12Resource *reductionBuffer, UINT reductionBufferLength)
{
	reductionBufferLength *= 2;	// to account for {avg, max} layout, RAW buffer view specify num of 32bit elements

	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const auto heapStart = allocation->GetCPUDescriptorHandleForHeapStart();

	// ZBufferSRV
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVdesc =
		{
			.Format = Config::ZFormat::shader,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2DMS{}
		};

		device->CreateShaderResourceView(ZBuffer, &SRVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, ZBufferSRV, descriptorSize));
	}

	// SrcSRV
	device->CreateShaderResourceView(src, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, SrcSRV, descriptorSize));

	// CompositeSRV
	device->CreateShaderResourceView(composite, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, CompositeSRV, descriptorSize));

	// CompositeUAV
	device->CreateUnorderedAccessView(composite, NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, CompositeUAV, descriptorSize));

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

	// COCBufferUAV
	device->CreateUnorderedAccessView(COCBuffer, NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, COCBufferUAV, descriptorSize));

	// HalfresDOFInputUAV
	device->CreateUnorderedAccessView(halferDOFSurface, NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, HalfresDOFInputUAV, descriptorSize));

	// DOFLayersUAV
	device->CreateUnorderedAccessView(DOFLayers, NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DOFLayersUAV, descriptorSize));

	// COCBufferSRV
	device->CreateShaderResourceView(COCBuffer, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, COCBufferSRV, descriptorSize));

	// HalfresDOFInputSRV
	device->CreateShaderResourceView(halferDOFSurface, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, HalfresDOFInputSRV, descriptorSize));

	// DOFLayersSRV
	device->CreateShaderResourceView(DOFLayers, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DOFLayersSRV, descriptorSize));

	// LensFlareSRV
	device->CreateShaderResourceView(lensFlareSurface, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, LensFlareSRV, descriptorSize));

	// BloomUpChainSRV
	device->CreateShaderResourceView(bloomUpChain, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, BloomUpChainSRV, descriptorSize));

	// BloomDownChainSRV
	device->CreateShaderResourceView(bloomDownChain, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, BloomDownChainSRV, descriptorSize));

	// Bloom chain UAVs
	{
		const auto bloomUpChainLen = bloomUpChain->GetDesc().MipLevels, bloomDownChainLen = bloomDownChain->GetDesc().MipLevels;
		assert(bloomUpChainLen <= 6);
		assert(bloomDownChainLen <= 5);

		// BloomUpChainUAVs
		for (UINT lod = 0, tableOffset = BloomUpChainUAVFirst; tableOffset <= BloomUpChainUAVLast; tableOffset++)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC UAVdesc =
			{
				DXGI_FORMAT_UNKNOWN,	// keep from resource
				D3D12_UAV_DIMENSION_TEXTURE2D
			};

			UAVdesc.Texture2D = { lod };

			device->CreateUnorderedAccessView(++/*1 mip goes in down chain*/lod < bloomUpChainLen ? bloomUpChain : NULL, NULL, &UAVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, tableOffset, descriptorSize));
		}

		// BloomDownChainUAVs
		for (UINT lod = 0, tableOffset = BloomDownChainUAVFirst; tableOffset <= BloomDownChainUAVLast; lod++, tableOffset++)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC UAVdesc =
			{
				DXGI_FORMAT_UNKNOWN,	// keep from resource
				D3D12_UAV_DIMENSION_TEXTURE2D
			};

			UAVdesc.Texture2D = { lod };

			ID3D12Resource *const bloomChainSelector = lod < bloomDownChainLen ? bloomDownChain : lod < bloomUpChainLen ? bloomUpChain : NULL;
			device->CreateUnorderedAccessView(bloomChainSelector, NULL, &UAVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, tableOffset, descriptorSize));
		}
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE PostprocessDescriptorTableStore::GetDescriptor(TableEntry entry) const
{
	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(allocation->GetCPUDescriptorHandleForHeapStart(), entry, descriptorSize);
}