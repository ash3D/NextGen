#include "stdafx.h"
#include "GPU texture sampler table.h"
#include "config.h"

using namespace Renderer::Impl::Descriptors;
using Microsoft::WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept;

ComPtr<ID3D12DescriptorHeap> TextureSampers::Impl::CreateHeap()
{
	ComPtr<ID3D12DescriptorHeap> result;

	// create
	{
		const D3D12_DESCRIPTOR_HEAP_DESC desc =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,			// type
			SAMPLER_TABLE_SIZE,							// count
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE	// GPU visible
		};
		CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(result.GetAddressOf())));
		NameObject(result.Get(), L"GPU texture sampler heap");
	}

	// fill
	{
		using namespace Config;
		const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		const auto heapStart = result->GetCPUDescriptorHandleForHeapStart();

		// terrain albedo
		{
			const D3D12_SAMPLER_DESC desc =
			{
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
				D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
				D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
				0,									// LOD bias
				Aniso::Terrain::albedo,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, TERRAIN_ALBEDO_SAMPLER, descriptorSize));
		}
	}

	return result;
}