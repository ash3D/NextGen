#include "stdafx.h"
#include "texture.hh"
#include "DMA engine.h"
#include "DDSTextureLoader12.h"

using namespace std;
using namespace Renderer;
using WRL::ComPtr;

decltype(Impl::Texture::pendingBarriers) Impl::Texture::pendingBarriers;

static inline pair<D3D12_RESOURCE_STATES, DirectX::DDS_LOADER_FLAGS> DecodeTextureUsage(TextureUsage usage)
{
	switch (usage)
	{
	case TextureUsage::TVScreen:
	case TextureUsage::AlbedoMap:
		return { D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, DirectX::DDS_LOADER_FORCE_SRGB };
	case TextureUsage::FresnelMap:
	case TextureUsage::RoughnessMap:
	case TextureUsage::NormalMap:
	case TextureUsage::GlassMask:
		return { D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, DirectX::DDS_LOADER_DEFAULT };
		break;
	default:
		throw invalid_argument("Invalid texture usage param.");
	}
}

// 1 call site
static inline void ValidateTexture(const D3D12_RESOURCE_DESC &desc, TextureUsage usage)
{
	// check dimension
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		throw logic_error("Trying to load texture with wrong dimension. 2D texture expected.");

	// check format for usage
	switch(usage)
	{
	case TextureUsage::TVScreen:
		switch (desc.Format)
		{
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			break;
		default:
			throw logic_error("Wrong texture format for TV screen.");
		}
		break;

	case TextureUsage::AlbedoMap:
		switch (desc.Format)
		{
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			break;
		default:
			throw logic_error("Wrong texture format for albedo map.");
		}
		break;

	case TextureUsage::FresnelMap:
		switch (desc.Format)
		{
		case DXGI_FORMAT_BC4_UNORM:
			break;
		default:
			throw logic_error("Wrong texture format for fresnel map.");
		}
		break;

	case TextureUsage::RoughnessMap:
		switch (desc.Format)
		{
		case DXGI_FORMAT_BC4_UNORM:
			break;
		default:
			throw logic_error("Wrong texture format for roughness map.");
		}
		break;

	case TextureUsage::NormalMap:
		switch (desc.Format)
		{
		case DXGI_FORMAT_BC5_SNORM:
			break;
		default:
			throw logic_error("Wrong texture format for normal map.");
		}
		break;

	case TextureUsage::GlassMask:
		switch (desc.Format)
		{
		case DXGI_FORMAT_BC4_UNORM:
			break;
		default:
			throw logic_error("Wrong texture format for glass mask.");
		}
		break;
	}
}

// out-of-line to break dependency on ComPtr`s copy ctor
Impl::Texture::operator Impl::TrackedResource<ID3D12Resource>() const
{
	return tex;
}

Impl::Texture::Texture() : usage{ ~0 }
{
}

Impl::Texture::Texture(const filesystem::path &fileName, TextureUsage usage, bool enablePacking, bool useSysRAM) : usage(usage)
{
	extern ComPtr<ID3D12Device2> device;
	using namespace DirectX;

	if (!useSysRAM)
	{
		D3D12_FEATURE_DATA_ARCHITECTURE GPUArch{};
		CheckHR(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &GPUArch, sizeof GPUArch));
		assert(GPUArch.UMA || !GPUArch.CacheCoherentUMA);
		useSysRAM = GPUArch.UMA;
		/*
		or CacheCoherentUMA ?
		or check device id ?
		https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/ns-d3d12-d3d12_feature_data_architecture#how-to-use-uma-and-cachecoherentuma
		*/
	}

	auto [targetState, loadFlags] = DecodeTextureUsage(usage);
#if 1
	if (enablePacking)
		reinterpret_cast<underlying_type_t<DDS_LOADER_FLAGS> &>(loadFlags) |= DDS_LOADER_ENABLE_PACKING;
#else
	reinterpret_cast<underlying_type_t<DDS_LOADER_FLAGS> &>(loadFlags) |= DDS_LOADER_ENABLE_PACKING & -enablePacking;
#endif

	// load from file & create texture in sys RAM
	unique_ptr<uint8_t []> data;
	vector<D3D12_SUBRESOURCE_DATA> subresources;
	CheckHR(LoadDDSTextureFromFileEx(device.Get(), fileName.c_str(), 0, D3D12_RESOURCE_FLAG_NONE, loadFlags, useSysRAM ? DDS_CPU_ACCESS_INDIRECT : DDS_CPU_ACCESS_DENY, tex.GetAddressOf(), data, subresources));
	ValidateTexture(tex->GetDesc(), usage);

	// write texture data
	if (useSysRAM)
	{
		//TODO: implement loop tiling optimization for cache-friendly access pattern (https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/nf-d3d12-id3d12resource-writetosubresource#remarks)
		for (unsigned mip = 0; mip < size(subresources); mip++)
		{
			CheckHR(tex->Map(mip, &CD3DX12_RANGE(0, 0), NULL));
			const auto &curMipData = subresources[mip];
			CheckHR(tex->WriteToSubresource(mip, NULL, curMipData.pData, curMipData.RowPitch, curMipData.SlicePitch));
			tex->Unmap(mip, NULL);
		}
	}
	else
		DMA::Upload2VRAM(tex, subresources, fileName.filename().c_str());

	// schedule transition to shader accessible state for next frame preparation stage
	static_assert(sizeof(decltype(pendingBarriers)::value_type::second_type) >= sizeof D3D12_RESOURCE_STATES);
	pendingBarriers.push_back({ tex, targetState });
}

Impl::Texture::Texture(const Texture &) = default;
Impl::Texture::Texture(Texture &&) = default;
Impl::Texture &Impl::Texture::operator =(const Texture &) = default;
Impl::Texture &Impl::Texture::operator =(Texture &&) = default;
Impl::Texture::~Texture() = default;

Impl::Texture::operator bool() const noexcept
{
	return tex;
}

// triggers ComPtr`s implementation if inline (dtor for temp object?)
auto Impl::Texture::AcquirePendingBarriers() noexcept -> decltype(pendingBarriers)
{
	return std::move(pendingBarriers);
}