#pragma once

#include "stdafx.h"
#include "object3D common samplers.hlsli"

namespace Renderer::Impl::Descriptors::TextureSamplers
{
	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		extern WRL::ComPtr<ID3D12DescriptorHeap> heap, CreateHeap();
	}

	enum DescriptorTableID
	{
		TERRAIN_DESC_TABLE_ID,
		OBJECT3D_DESC_TABLE_ID,
		DESC_TABLES_COUNT
	};

	enum
	{
		OBJ3D_TV_RANGE_ID,
		OBJ3D_COMMON_RANGE_ID,
		OBJ3D_COMMON_TILED_RANGE_ID,
		OBJ3D_RANGES_COUNT
	};

	enum
	{
		TERRAIN_ALBEDO_SAMPLER,
		TERRAIN_FRESNEL_SAMPLER,
		TERRAIN_ROUGHNESS_SAMPLER,
		TERRAIN_BUMP_SAMPLER,
		TERRAIN_SAMPLERS_COUNT
	};

	inline const WRL::ComPtr<ID3D12DescriptorHeap> &GetHeap() noexcept { return Impl::heap; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUAddress(DescriptorTableID id);
	CD3DX12_ROOT_PARAMETER1 GetDescTable(DescriptorTableID id, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
}