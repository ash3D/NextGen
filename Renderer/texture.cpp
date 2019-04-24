#include "stdafx.h"
#include "texture.hh"
#include "DDSTextureLoader12.h"

using namespace std;
using namespace Renderer;
using WRL::ComPtr;

decltype(Impl::Texture::pendingBarriers) Impl::Texture::pendingBarriers;

// out-of-line to break dependency on ComPtr`s copy ctor
Impl::Texture::operator Impl::TrackedResource<ID3D12Resource>() const
{
	return tex;
}

Impl::Texture::Texture(const filesystem::path &fileName)
{
	extern ComPtr<ID3D12Device2> device;

	// load from file & create texture in sys RAM
	unique_ptr<uint8_t []> data;
	vector<D3D12_SUBRESOURCE_DATA> subresources;
	ComPtr<ID3D12Resource> dummy;
	CheckHR(DirectX::LoadDDSTextureFromFile(device.Get(), fileName.c_str(), dummy.GetAddressOf(), data, subresources));
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE, D3D12_MEMORY_POOL_L0),
		D3D12_HEAP_FLAG_NONE,
		&dummy->GetDesc(),
		D3D12_RESOURCE_STATE_COMMON,	// required for 'WriteToSubresource()'
		NULL,							// clear value
		IID_PPV_ARGS(tex.GetAddressOf())));

	// write texture data\
	TODO: implement loop tiling optimization for cache-friendly access pattern (https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/nf-d3d12-id3d12resource-writetosubresource#remarks)
	for (unsigned mip = 0; mip < size(subresources); mip++)
	{
		CheckHR(tex->Map(mip, &CD3DX12_RANGE(0, 0), NULL));
		const auto &curMipData = subresources[mip];
		CheckHR(tex->WriteToSubresource(mip, NULL, curMipData.pData, curMipData.RowPitch, curMipData.SlicePitch));
		tex->Unmap(mip, NULL);
	}

	// plan transition to shader accessibble state for next frame preparation stage
	static_assert(sizeof(decltype(pendingBarriers)::value_type::second_type) >= sizeof D3D12_RESOURCE_STATES);
	pendingBarriers.push_back({ tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE });
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