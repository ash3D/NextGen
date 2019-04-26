#include "stdafx.h"
#include "texture.hh"
#include "DDSTextureLoader12.h"

using namespace std;
using namespace Renderer;
using WRL::ComPtr;

decltype(Impl::Texture::pendingBarriers) Impl::Texture::pendingBarriers;

static inline pair<D3D12_RESOURCE_STATES, DirectX::DDS_LOADER_FLAGS> DecodeTextureUsage(TextureUsage usage)
{
	switch (usage)
	{
	case TextureUsage::AlbedoMap:
		return { D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, DirectX::DDS_LOADER_FORCE_SRGB };
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
		throw invalid_argument("Trying to load texture with wrong dimension. 2D texture expected.");

	// check format for usage
	switch(usage)
	{
	case TextureUsage::AlbedoMap:
		switch (desc.Format)
		{
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
		case DXGI_FORMAT_B4G4R4A4_UNORM:
			break;
		default:
			throw invalid_argument("Wrong texture format for albedo map.");
		}
		break;
	}
}

// out-of-line to break dependency on ComPtr`s copy ctor
Impl::Texture::operator Impl::TrackedResource<ID3D12Resource>() const
{
	return tex;
}

Impl::Texture::Texture(const filesystem::path &fileName, TextureUsage usage) : usage(usage)
{
	extern ComPtr<ID3D12Device2> device;

	const auto [targetState, loadFlags] = DecodeTextureUsage(usage);

	// load from file & create texture in sys RAM
	unique_ptr<uint8_t []> data;
	vector<D3D12_SUBRESOURCE_DATA> subresources;
	using namespace DirectX;
	CheckHR(LoadDDSTextureFromFileEx(device.Get(), fileName.c_str(), 0, D3D12_RESOURCE_FLAG_NONE, loadFlags, DDS_CPU_ACCESS_INDIRECT, tex.GetAddressOf(), data, subresources));
	ValidateTexture(tex->GetDesc(), usage);

	// write texture data\
	TODO: implement loop tiling optimization for cache-friendly access pattern (https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/nf-d3d12-id3d12resource-writetosubresource#remarks)
	for (unsigned mip = 0; mip < size(subresources); mip++)
	{
		CheckHR(tex->Map(mip, &CD3DX12_RANGE(0, 0), NULL));
		const auto &curMipData = subresources[mip];
		CheckHR(tex->WriteToSubresource(mip, NULL, curMipData.pData, curMipData.RowPitch, curMipData.SlicePitch));
		tex->Unmap(mip, NULL);
	}

	// schedule transition to shader accessibble state for next frame preparation stage
	static_assert(sizeof(decltype(pendingBarriers)::value_type::second_type) >= sizeof D3D12_RESOURCE_STATES);
	pendingBarriers.push_back({ tex, targetState });
}

Impl::Texture::Texture(const Texture &) = default;
Impl::Texture::Texture(Texture &&) = default;
Impl::Texture &Impl::Texture::operator =(const Texture &) = default;
Impl::Texture &Impl::Texture::operator =(Texture &&) = default;
Impl::Texture::~Texture() = default;

// triggers ComPtr`s impementation if inline (dtor for temp object?)
auto Impl::Texture::AcquirePendingBarriers() noexcept -> decltype(pendingBarriers)
{
	return std::move(pendingBarriers);
}