#include "stdafx.h"
#include "postprocess descriptor table store.h"
#include "offscreen buffers.h"
#include "config.h"

using namespace Renderer::Impl::Descriptors;
using WRL::ComPtr;

extern ComPtr<ID3D12Device4> device;
	
PostprocessDescriptorTableStore::PostprocessDescriptorTableStore()
{
	void NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

	const D3D12_DESCRIPTOR_HEAP_DESC heapDesc =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	// type
		TableSize,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE			// CPU visible
	};
	CheckHR(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(CPUStore.GetAddressOf())));
	NameObjectF(CPUStore.Get(), L"CPU descriptor stage for postprocess buffers (D3D object: %p, heap start CPU address: %p)", CPUStore.Get(), CPUStore->GetCPUDescriptorHandleForHeapStart());
}

void PostprocessDescriptorTableStore::Fill(const OffscreenBuffers &offscreenBuffers, UINT reductionBufferLength)
{
	reductionBufferLength *= 2;	// to account for {avg, max} layout, RAW buffer view specify num of 32bit elements

	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const auto heapStart = CPUStore->GetCPUDescriptorHandleForHeapStart();

	// ZBufferSRV
	{
		const D3D12_SHADER_RESOURCE_VIEW_DESC SRVdesc =
		{
			.Format = Config::ZFormat::shader,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2DMS{}
		};

		device->CreateShaderResourceView(offscreenBuffers.GetROPsBuffers().persistent.ZBuffer.resource.Get(), &SRVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, ZBufferSRV, descriptorSize));
	}

	// SrcSRV
	device->CreateShaderResourceView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.bokeh.HDRInputSurface.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, SrcSRV, descriptorSize));

	// CompositeSRV
	device->CreateShaderResourceView(offscreenBuffers.GetShaderOnlyBuffers().persistent.HDRCompositeSurface.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, CompositeSRV, descriptorSize));

	// CompositeUAV
	device->CreateUnorderedAccessView(offscreenBuffers.GetShaderOnlyBuffers().persistent.HDRCompositeSurface.resource.Get(), NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, CompositeUAV, descriptorSize));

	// DstUAV
	device->CreateUnorderedAccessView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.lum.LDRSurface.resource.Get(), NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DstUAV, descriptorSize));

	// ReductionBufferUAV
	{
		const D3D12_UNORDERED_ACCESS_VIEW_DESC UAVdesc =
		{
			.Format = DXGI_FORMAT_R32_TYPELESS,	// RAW buffer
			.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
			.Buffer
			{
				.NumElements = reductionBufferLength,
				.Flags = D3D12_BUFFER_UAV_FLAG_RAW
			}
		};
		
		device->CreateUnorderedAccessView(offscreenBuffers.GetLuminanceReductionBuffer().Get(), NULL, &UAVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, ReductionBufferUAV, descriptorSize));
	}

	// ReductionBufferSRV
	{
		const D3D12_SHADER_RESOURCE_VIEW_DESC SRVdesc =
		{
			.Format = DXGI_FORMAT_R32_TYPELESS,	// RAW buffer
			.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Buffer
			{
				.NumElements = reductionBufferLength,
				.Flags = D3D12_BUFFER_SRV_FLAG_RAW
			}
		};

		device->CreateShaderResourceView(offscreenBuffers.GetLuminanceReductionBuffer().Get(), &SRVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, ReductionBufferSRV, descriptorSize));
	}

	// DOFOpacityBufferUAV
	device->CreateUnorderedAccessView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.bokeh.DOFOpacityBuffer.resource.Get(), NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DOFOpacityBufferUAV, descriptorSize));

	// COCBufferUAV
	device->CreateUnorderedAccessView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.bokeh.COCBuffer.resource.Get(), NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, COCBufferUAV, descriptorSize));

	// DilatedCOCBufferUAV
	device->CreateUnorderedAccessView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.bokeh.dilatedCOCBuffer.resource.Get(), NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DilatedCOCBufferUAV, descriptorSize));

	// HalfresDOFInputUAV
	device->CreateUnorderedAccessView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.bokeh.halfresDOFSurface.resource.Get(), NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, HalfresDOFInputUAV, descriptorSize));

	// DOFLayersUAV
	device->CreateUnorderedAccessView(offscreenBuffers.GetROPsBuffers().overlapped.postFX.DOFLayers.resource.Get(), NULL, NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DOFLayersUAV, descriptorSize));

	// DOFOpacityBufferSRV
	device->CreateShaderResourceView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.bokeh.DOFOpacityBuffer.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DOFOpacityBufferSRV, descriptorSize));

	// COCBufferSRV
	device->CreateShaderResourceView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.bokeh.COCBuffer.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, COCBufferSRV, descriptorSize));

	// DilatedCOCBufferSRV
	device->CreateShaderResourceView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.bokeh.dilatedCOCBuffer.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DilatedCOCBufferSRV, descriptorSize));

	// HalfresDOFInputSRV
	device->CreateShaderResourceView(offscreenBuffers.GetShaderOnlyBuffers().overlapped.bokeh.halfresDOFSurface.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, HalfresDOFInputSRV, descriptorSize));

	// DOFLayersSRV
	device->CreateShaderResourceView(offscreenBuffers.GetROPsBuffers().overlapped.postFX.DOFLayers.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, DOFLayersSRV, descriptorSize));

	// LensFlareSRV
	device->CreateShaderResourceView(offscreenBuffers.GetROPsBuffers().overlapped.postFX.lensFlareSurface.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, LensFlareSRV, descriptorSize));

	ID3D12Resource *const bloomUpChain = offscreenBuffers.GetShaderOnlyBuffers().overlapped.lum.bloomUpChain.resource.Get(), *const bloomDownChain = offscreenBuffers.GetShaderOnlyBuffers().overlapped.lum.bloomDownChain.resource.Get();

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
			const D3D12_UNORDERED_ACCESS_VIEW_DESC UAVdesc =
			{
				.Format = DXGI_FORMAT_UNKNOWN,	// keep from resource
				.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
				.Texture2D = { lod }
			};

			device->CreateUnorderedAccessView(++/*1 mip goes in down chain*/lod < bloomUpChainLen ? bloomUpChain : NULL, NULL, &UAVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, tableOffset, descriptorSize));
		}

		// BloomDownChainUAVs
		for (UINT lod = 0, tableOffset = BloomDownChainUAVFirst; tableOffset <= BloomDownChainUAVLast; lod++, tableOffset++)
		{
			const D3D12_UNORDERED_ACCESS_VIEW_DESC UAVdesc =
			{
				.Format = DXGI_FORMAT_UNKNOWN,	// keep from resource
				.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
				.Texture2D = { lod }
			};

			ID3D12Resource *const bloomChainSelector = lod < bloomDownChainLen ? bloomDownChain : lod < bloomUpChainLen ? bloomUpChain : NULL;
			device->CreateUnorderedAccessView(bloomChainSelector, NULL, &UAVdesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, tableOffset, descriptorSize));
		}
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE PostprocessDescriptorTableStore::GetDescriptor(TableEntry entry) const
{
	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(CPUStore->GetCPUDescriptorHandleForHeapStart(), entry, descriptorSize);
}