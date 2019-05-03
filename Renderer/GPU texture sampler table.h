#pragma once

#include "stdafx.h"

namespace Renderer::Impl::Descriptors::TextureSampers
{
	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		extern WRL::ComPtr<ID3D12DescriptorHeap> heap, CreateHeap();
	}

	inline const WRL::ComPtr<ID3D12DescriptorHeap> &GetHeap() noexcept { return Impl::heap; }

	enum
	{
		TERRAIN_ALBEDO_SAMPLER,
		TERRAIN_FRESNEL_SAMPLER,
		TERRAIN_ROUGHNESS_SAMPLER,
		TERRAIN_BUMP_SAMPLER,
		SAMPLER_TABLE_SIZE
	};

	inline CD3DX12_ROOT_PARAMETER1 GetDescTable(D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		// must be static since its pointer being returned
		static const CD3DX12_DESCRIPTOR_RANGE1 descTable(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, SAMPLER_TABLE_SIZE, 0);
		CD3DX12_ROOT_PARAMETER1 rootParam;
		rootParam.InitAsDescriptorTable(1, &descTable, visibility);
		return rootParam;
	};
}