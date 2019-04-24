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

// out-of-line to break dependency on ComPtr`s copy ctor
Impl::Texture::operator Impl::TrackedResource<ID3D12Resource>() const
{
	return tex;
}

Impl::Texture::Texture(const filesystem::path &fileName, TextureUsage usage)
{
	extern ComPtr<ID3D12Device2> device;

	const auto [targetState, loadFlags] = DecodeTextureUsage(usage);

	// load from file & create texture in sys RAM
	unique_ptr<uint8_t []> data;
	vector<D3D12_SUBRESOURCE_DATA> subresources;
	using namespace DirectX;
	CheckHR(LoadDDSTextureFromFileEx(device.Get(), fileName.c_str(), 0, D3D12_RESOURCE_FLAG_NONE, loadFlags, DDS_CPU_ACCESS_INDIRECT, tex.GetAddressOf(), data, subresources));

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