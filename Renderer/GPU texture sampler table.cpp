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
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				0,									// LOD bias
				Aniso::Terrain::albedo,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, TERRAIN_ALBEDO_SAMPLER, descriptorSize));
		}

		// terrain fresnel
		{
			const D3D12_SAMPLER_DESC desc =
			{
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				0,									// LOD bias
				Aniso::Terrain::fresnel,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, TERRAIN_FRESNEL_SAMPLER, descriptorSize));
		}

		// terrain roughness
		{
			const D3D12_SAMPLER_DESC desc =
			{
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				0,									// LOD bias
				Aniso::Terrain::roughness,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, TERRAIN_ROUGHNESS_SAMPLER, descriptorSize));
		}

		// terrain bump
		{
			const D3D12_SAMPLER_DESC desc =
			{
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				0,									// LOD bias
				Aniso::Terrain::bump,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, TERRAIN_BUMP_SAMPLER, descriptorSize));
		}

		// 3D object albedo
		{
			const D3D12_SAMPLER_DESC desc =
			{
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				0,									// LOD bias
				Aniso::Object3D::albedo,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, OBJ3D_ALBEDO_SAMPLER, descriptorSize));
		}

		// 3D object albedo with alphatest
		{
			const D3D12_SAMPLER_DESC desc =
			{
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				-log2(sqrt(Config::MSAA().Count)),	// neative LOD bias to account for supersampled alphatest
				Aniso::Object3D::albedo,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, TERRAIN_ALBEDO_ALPHATEST_SAMPLER, descriptorSize));
		}

		// 3D object bump
		{
			const D3D12_SAMPLER_DESC desc =
			{
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				0,									// LOD bias
				Aniso::Object3D::bump,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, OBJ3D_BUMP_SAMPLER, descriptorSize));
		}

		// 3D object glass mask
		{
			const D3D12_SAMPLER_DESC desc =
			{
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				0,									// LOD bias
				Aniso::Object3D::glassMask,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, OBJ3D_GLASS_MASK_SAMPLER, descriptorSize));
		}

		// 3D object TV
		{
			const D3D12_SAMPLER_DESC desc =
			{
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				0,									// LOD bias
				Aniso::Object3D::TV,
				D3D12_COMPARISON_FUNC_NEVER,
				{},									// border color
				-D3D12_FLOAT32_MAX,					// min LOD
				+D3D12_FLOAT32_MAX					// max LOD
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, OBJ3D_TV_SAMPLER, descriptorSize));
		}
	}

	return result;
}