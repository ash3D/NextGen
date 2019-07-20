#include "stdafx.h"
#include "GPU texture sampler tables.h"
#include "config.h"

using namespace Renderer::Impl::Descriptors;
using Microsoft::WRL::ComPtr;

extern ComPtr<ID3D12Device2> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept;

// consider consteval accumulate/prefix sum
constexpr unsigned obj3DSizeTable[TextureSamplers::OBJ3D_RANGES_COUNT] =
{
	1,												// TV
	TextureSamplers::OBJ3D_COMMON_SAMPLERS_COUNT,
	TextureSamplers::OBJ3D_COMMON_SAMPLERS_COUNT	// tiled
}, obj3DSuballocationTable[TextureSamplers::OBJ3D_RANGES_COUNT] =
{
	0,
	obj3DSizeTable[0],
	obj3DSizeTable[0] + obj3DSizeTable[1]
}, sizeTable[TextureSamplers::DESC_TABLES_COUNT] =
{
	TextureSamplers::TERRAIN_SAMPLERS_COUNT,
	obj3DSizeTable[0] + obj3DSizeTable[1] + obj3DSizeTable[2]
}, allocationTable[TextureSamplers::DESC_TABLES_COUNT] =
{
	0,												// terrain
	sizeTable[0]									// 3D object
};

ComPtr<ID3D12DescriptorHeap> TextureSamplers::Impl::CreateHeap()
{
	ComPtr<ID3D12DescriptorHeap> result;

	// create
	{
		const D3D12_DESCRIPTOR_HEAP_DESC desc =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,			// type
			sizeTable[0] + sizeTable[1],				// count
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

#pragma region terrain
		// terrain albedo
		{
			const D3D12_SAMPLER_DESC desc =
			{
				.Filter = D3D12_FILTER_ANISOTROPIC,
				.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MaxAnisotropy = Aniso::Terrain::albedo,
				.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
				.MinLOD = -D3D12_FLOAT32_MAX,
				.MaxLOD = +D3D12_FLOAT32_MAX
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, allocationTable[TERRAIN_DESC_TABLE_ID] + TERRAIN_ALBEDO_SAMPLER, descriptorSize));
		}

		// terrain fresnel
		{
			const D3D12_SAMPLER_DESC desc =
			{
				.Filter = D3D12_FILTER_ANISOTROPIC,
				.AddressU =D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MaxAnisotropy = Aniso::Terrain::fresnel,
				.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
				.MinLOD = -D3D12_FLOAT32_MAX,
				.MaxLOD = +D3D12_FLOAT32_MAX
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, allocationTable[TERRAIN_DESC_TABLE_ID] + TERRAIN_FRESNEL_SAMPLER, descriptorSize));
		}

		// terrain roughness
		{
			const D3D12_SAMPLER_DESC desc =
			{
				.Filter = D3D12_FILTER_ANISOTROPIC,
				.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MaxAnisotropy = Aniso::Terrain::roughness,
				.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
				.MinLOD = -D3D12_FLOAT32_MAX,
				.MaxLOD = +D3D12_FLOAT32_MAX
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, allocationTable[TERRAIN_DESC_TABLE_ID] + TERRAIN_ROUGHNESS_SAMPLER, descriptorSize));
		}

		// terrain bump
		{
			const D3D12_SAMPLER_DESC desc =
			{
				.Filter = D3D12_FILTER_ANISOTROPIC,
				.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MaxAnisotropy = Aniso::Terrain::bump,
				.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
				.MinLOD = -D3D12_FLOAT32_MAX,
				.MaxLOD = +D3D12_FLOAT32_MAX
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, allocationTable[TERRAIN_DESC_TABLE_ID] + TERRAIN_BUMP_SAMPLER, descriptorSize));
		}
#pragma endregion

#pragma region 3D object
		// 3D object TV
		{
			const D3D12_SAMPLER_DESC desc =
			{
				.Filter = D3D12_FILTER_ANISOTROPIC,
				.AddressU =D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
				.MaxAnisotropy = Aniso::Object3D::TV,
				.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
				.MinLOD = -D3D12_FLOAT32_MAX,
				.MaxLOD = +D3D12_FLOAT32_MAX
			};
			device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, allocationTable[OBJECT3D_DESC_TABLE_ID] + obj3DSuballocationTable[OBJ3D_TV_RANGE_ID], descriptorSize));
		}

		const auto FillObj3DSamplers = [=](unsigned allocationOffset, D3D12_TEXTURE_ADDRESS_MODE addressMode)
		{
			// 3D object albedo
			{
				const D3D12_SAMPLER_DESC desc =
				{
					.Filter = D3D12_FILTER_ANISOTROPIC,
					.AddressU =addressMode,
					.AddressV = addressMode,
					.AddressW = addressMode,
					.MaxAnisotropy = Aniso::Object3D::albedo,
					.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
					.MinLOD = -D3D12_FLOAT32_MAX,
					.MaxLOD = +D3D12_FLOAT32_MAX
				};
				device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, allocationOffset + OBJ3D_ALBEDO_SAMPLER, descriptorSize));
			}

			// 3D object albedo with alphatest
			{
				const D3D12_SAMPLER_DESC desc =
				{
					.Filter = D3D12_FILTER_ANISOTROPIC,
					.AddressU = addressMode,
					.AddressV = addressMode,
					.AddressW = addressMode,
					.MipLODBias = -log2f(sqrtf(Config::MSAA().Count)),	// negative LOD bias to account for supersampled alphatest
					.MaxAnisotropy = Aniso::Object3D::albedo,
					.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
					.MinLOD = -D3D12_FLOAT32_MAX,
					.MaxLOD = +D3D12_FLOAT32_MAX
				};
				device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, allocationOffset + OBJ3D_ALBEDO_ALPHATEST_SAMPLER, descriptorSize));
			}

			// 3D object bump
			{
				const D3D12_SAMPLER_DESC desc =
				{
					.Filter = D3D12_FILTER_ANISOTROPIC,
					.AddressU = addressMode,
					.AddressV = addressMode,
					.AddressW = addressMode,
					.MaxAnisotropy = Aniso::Object3D::bump,
					.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
					.MinLOD = -D3D12_FLOAT32_MAX,
					.MaxLOD = +D3D12_FLOAT32_MAX
				};
				device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, allocationOffset + OBJ3D_BUMP_SAMPLER, descriptorSize));
			}

			// 3D object glass mask
			{
				const D3D12_SAMPLER_DESC desc =
				{
					.Filter = D3D12_FILTER_ANISOTROPIC,
					.AddressU = addressMode,
					.AddressV = addressMode,
					.AddressW = addressMode,
					.MaxAnisotropy = Aniso::Object3D::glassMask,
					.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
					.MinLOD = -D3D12_FLOAT32_MAX,
					.MaxLOD = +D3D12_FLOAT32_MAX
				};
				device->CreateSampler(&desc, CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStart, allocationOffset + OBJ3D_GLASS_MASK_SAMPLER, descriptorSize));
			}
		};

		FillObj3DSamplers(allocationTable[OBJECT3D_DESC_TABLE_ID] + obj3DSuballocationTable[OBJ3D_COMMON_RANGE_ID], D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		FillObj3DSamplers(allocationTable[OBJECT3D_DESC_TABLE_ID] + obj3DSuballocationTable[OBJ3D_COMMON_TILED_RANGE_ID], D3D12_TEXTURE_ADDRESS_MODE_WRAP);
#pragma endregion
	}

	return result;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureSamplers::GetGPUAddress(DescriptorTableID id)
{
	const auto descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(GetHeap()->GetGPUDescriptorHandleForHeapStart(), allocationTable[id], descriptorSize);
}

CD3DX12_ROOT_PARAMETER1 TextureSamplers::GetDescTable(DescriptorTableID id, D3D12_SHADER_VISIBILITY visibility)
{
	// must be static since its pointer being returned
	static const CD3DX12_DESCRIPTOR_RANGE1 descTables[DESC_TABLES_COUNT]
	{
		{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, sizeTable[0], 0},
		{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, sizeTable[1], 0}
	};
	CD3DX12_ROOT_PARAMETER1 rootParam;
	rootParam.InitAsDescriptorTable(1, descTables + id, visibility);
	return rootParam;
}