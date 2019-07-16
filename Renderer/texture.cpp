#include "stdafx.h"
#include "texture.hh"
#include "DMA engine.h"
#include "system.h"
#include "DDSTextureLoader12.h"

#define FORCE_PARALLEL_IO 0

/*
Design considerations on async loading architecture:
	Current implementation is rather glimpse on "final perfect" architecture that hopefully will be implemented somewhen in future.
	This "perfect" arch will initially load small mips and then stream full-lod texs based on priority queue.
	Priority will be calculated based on "importance" of texture type and visibility/frequency of access (usage in several objects, occlusion query results etc).
	Current std::[shared_]future approach is quite limited in providing asynchrony meaning that game level loader will have to wait for loading to complete
		when it's time to passing texture to object constructor that needs it (terrain material, 3D object).
	So game can do something concurrently with texture loading during this time period [start texture loading .. create textured object].
	The reason why asynchrony implemented in such manner instead of doing async internally (return Texture instead of [shared_]future<Texture>) and blocking when
		texture is really needed to render frame is error reporting considerations - game can retrieve loading errors (file not found, format/usage validation fails)
		and respond appropriately (e.g. use different material).
	In "future perfect" design validation reporting will happen for small mips only while big ones will not block game loader,
		error reporting for them is not required since small ones was already loaded successfully.
	So blocking can happen only for initial small mips loads which is not critical. Moreover async perhaps is not needed for small mips at all.
	Although current [shared_]future mechanism can be employed for them.
*/

using namespace std;
using namespace Renderer;
using WRL::ComPtr;

vector<Impl::Texture::PendingLoad> Impl::Texture::pendingLoads;

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
	extern ComPtr<ID3D12CommandQueue> dmaQueue;
	using namespace DirectX;

	useSysRAM |= !dmaQueue;

	auto [targetState, loadFlags] = DecodeTextureUsage(usage);
#if 1
	if (enablePacking)
		reinterpret_cast<underlying_type_t<DDS_LOADER_FLAGS> &>(loadFlags) |= DDS_LOADER_ENABLE_PACKING;
#else
	reinterpret_cast<underlying_type_t<DDS_LOADER_FLAGS> &>(loadFlags) |= DDS_LOADER_ENABLE_PACKING & -enablePacking;
#endif
#if !FORCE_PARALLEL_IO
	if (!System::DetectSSD(fileName))
		reinterpret_cast<underlying_type_t<DDS_LOADER_FLAGS> &>(loadFlags) |= DDS_LOADER_THROTTLE_IO;
#endif

	// load from file & create texture
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
}

// not thread-safe
shared_future<::Texture> __cdecl Impl::Texture::LoadAsync(filesystem::path fileName, TextureUsage usage, bool enablePacking, bool forceSysRAM)
{
	auto args = make_tuple(move(fileName), usage, enablePacking, forceSysRAM);
	pendingLoads.emplace_back(async(make_from_tuple<::Texture, decltype(args)>, move(args)));
	return pendingLoads.back();
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

// static container, no temps (seemingly, maybe STL does make), thus no stack unwinding so no problems for exceptions in dtor (although it breaks STL`s exception safety guarantee)
Impl::Texture::PendingLoad::~PendingLoad() noexcept(false)
{
	if (valid())
		wait();
}

void Impl::Texture::WaitForPendingLoads()
{
	pendingLoads.clear();
}

bool Impl::Texture::PendingLoadsCompleted()
{
	const bool completed = all_of(pendingLoads.cbegin(), pendingLoads.cend(), [](const PendingLoad &load) { return load.wait_for(0s) == future_status::ready; });
	if (completed)
		pendingLoads.clear();
	return completed;
}