#include "stdafx.h"
#include "offscreen buffers.h"
#include "tracked resource.inl"
#include "config.h"
#include "CS config.h"
#include "align.h"

using namespace std;
using namespace Renderer::Impl;
using WRL::ComPtr;

extern ComPtr<ID3D12Device4> device;
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept;

static constexpr UINT64 shrinkThreshold = 16'777'216ULL;

class Renderer::Impl::OffscreenBuffersDesc final
{
	Math::VectorMath::HLSL::uint2 fullres, halfres;
	unsigned long bloomUpChainLen, bloomDownChainLen;
	DXGI_SAMPLE_DESC MSAA_mode = Config::MSAA();

public:
	explicit OffscreenBuffersDesc(UINT width, UINT height);

public:
	D3D12_RESOURCE_DESC ZBuffer() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::ZFormat::ROP, fullres.x, fullres.y, 1, 1, MSAA_mode.Count, MSAA_mode.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	}

	D3D12_RESOURCE_DESC Rendertarget() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, fullres.x, fullres.y, 1, 1, MSAA_mode.Count, MSAA_mode.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	}

	D3D12_RESOURCE_DESC DOFLayers() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::DOFLayersFormat, halfres.x, halfres.y, 4, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	}

	D3D12_RESOURCE_DESC LensFlareSurface() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, halfres.x, halfres.y, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	}

	D3D12_RESOURCE_DESC HDRCompositeSurface() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, fullres.x, fullres.y, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	D3D12_RESOURCE_DESC HDRInputSurface() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, fullres.x, fullres.y, 1, 1);
	}

	D3D12_RESOURCE_DESC DOFOpacityBuffer() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16_UNORM, fullres.x, fullres.y, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	D3D12_RESOURCE_DESC COCBuffer() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16_FLOAT, fullres.x, fullres.y, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	D3D12_RESOURCE_DESC DilatedCOCBuffer() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16_FLOAT, halfres.x, halfres.y, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	D3D12_RESOURCE_DESC HalfresDOFSurface() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, halfres.x, halfres.y, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	D3D12_RESOURCE_DESC BloomUpChain() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, fullres.x / 2, fullres.y / 2, 1, bloomUpChainLen, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	D3D12_RESOURCE_DESC BloomDownChain() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::HDRFormat, fullres.x / 2, fullres.y / 2, 1, bloomDownChainLen, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	D3D12_RESOURCE_DESC LDRSurface() const noexcept
	{
		return CD3DX12_RESOURCE_DESC::Tex2D(Config::DisplayFormat, fullres.x, fullres.y, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}
};

OffscreenBuffersDesc::OffscreenBuffersDesc(UINT width, UINT height) : fullres(width, height), halfres((fullres + 1) / 2)
{
	// TODO: replace with C++20 std::log2p1 with halfres
	_BitScanReverse(&bloomUpChainLen, max(width, height));
	bloomUpChainLen = min(bloomUpChainLen, 6ul);
	bloomDownChainLen = bloomUpChainLen - 1;
}

decltype(OffscreenBuffers::store) OffscreenBuffers::store;

namespace Renderer::Impl::OffscreenBuffersLayout
{
	// distinguish ROPs/Shaders heaps via allocation's offset MSB
	static constexpr UINT64 heapOffset = UINT64_C(1) << numeric_limits<UINT64>::digits - 1;

	static inline UINT64 RemoveHeapOffset(UINT64 allocationOffset) noexcept
	{
		return allocationOffset & ~heapOffset;
	}

	namespace Tier_1
	{
		static void CheckOverflow(const OffscreenBuffers::AllocatedResource &allocation)
		{
			if (allocation.offset + allocation.size > heapOffset)
				throw overflow_error("Offscreen buffers heap overflow.");
		}

		namespace ROPs
		{
			static inline void ApplyHeapOffset(OffscreenBuffers::AllocatedResource &allocation)
			{
				CheckOverflow(allocation);
				// NOP - zero offset
			}

			namespace Persistent
			{
				template<typename LifetimeRanges>
				static inline UINT64 ZBuffer(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRanges &dst)
				{
					const D3D12_RESOURCE_ALLOCATION_INFO ZBufferAllocation = device->GetResourceAllocationInfo(0, 1, &offscreenBuffersDesc.ZBuffer());
					dst.persistent.ZBuffer.offset = 0;
					dst.persistent.ZBuffer.size = ZBufferAllocation.SizeInBytes;
					ApplyHeapOffset(dst.persistent.ZBuffer);
					return ZBufferAllocation.SizeInBytes;
				}
			}

			namespace Overlapped
			{
				enum
				{
					PostFXLayout_DOFLayers,
					PostFXLayout_LensFlareSurface,
					PostFXLayout_BuffersCount
				};

				template<typename LifetimeRanges>
				static inline UINT64 Rendertarget(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRanges &dst, UINT64 baseOffset)
				{
					const D3D12_RESOURCE_ALLOCATION_INFO rendertargetAllocation = device->GetResourceAllocationInfo(0, 1, &offscreenBuffersDesc.Rendertarget());
					dst.world.rendertarget.offset = AlignSize(baseOffset, rendertargetAllocation.Alignment);
					dst.world.rendertarget.size = rendertargetAllocation.SizeInBytes;
					const auto rendertargetPadding = dst.world.rendertarget.offset/*aligned*/ - baseOffset;
					ApplyHeapOffset(dst.world.rendertarget);
					return rendertargetPadding + rendertargetAllocation.SizeInBytes;
				}

				template<typename LifetimeRanges>
				static inline UINT64 PostFX(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRanges &dst, UINT64 baseOffset)
				{
					D3D12_RESOURCE_DESC postFXBuffersDesc[PostFXLayout_BuffersCount];
					postFXBuffersDesc[PostFXLayout_DOFLayers] = offscreenBuffersDesc.DOFLayers();
					postFXBuffersDesc[PostFXLayout_LensFlareSurface] = offscreenBuffersDesc.LensFlareSurface();
					D3D12_RESOURCE_ALLOCATION_INFO1 postFXSuballocation[PostFXLayout_BuffersCount];
					const D3D12_RESOURCE_ALLOCATION_INFO postFXAllocation = device->GetResourceAllocationInfo1(0, PostFXLayout_BuffersCount, postFXBuffersDesc, postFXSuballocation);
					const auto alignedPostFXOffset = AlignSize(baseOffset, postFXAllocation.Alignment);
					const auto postFXPadding = alignedPostFXOffset - baseOffset;
					dst.postFX_1.DOFLayers.offset = alignedPostFXOffset + postFXSuballocation[PostFXLayout_DOFLayers].Offset;
					dst.postFX_1.DOFLayers.size = postFXSuballocation[PostFXLayout_DOFLayers].SizeInBytes;
					ApplyHeapOffset(dst.postFX_1.DOFLayers);
					dst.postFX_1.lensFlareSurface.offset = alignedPostFXOffset + postFXSuballocation[PostFXLayout_LensFlareSurface].Offset;
					dst.postFX_1.lensFlareSurface.size = postFXSuballocation[PostFXLayout_LensFlareSurface].SizeInBytes;
					ApplyHeapOffset(dst.postFX_1.lensFlareSurface);
					return postFXPadding + postFXAllocation.SizeInBytes;
				}
			}
		}

		namespace Shaders
		{
			inline void ApplyHeapOffset(OffscreenBuffers::AllocatedResource &allocation)
			{
				CheckOverflow(allocation);
				allocation.offset |= heapOffset;
			}

			namespace Persistent
			{
				template<typename LifetimeRanges>
				static inline UINT64 HDRCompositeSurface(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRanges &dst)
				{
					const D3D12_RESOURCE_ALLOCATION_INFO HDRCompositeSurfaceAllocation = device->GetResourceAllocationInfo(0, 1, &offscreenBuffersDesc.HDRCompositeSurface());
					dst.postFX.HDRCompositeSurface.offset = 0;
					dst.postFX.HDRCompositeSurface.size = HDRCompositeSurfaceAllocation.SizeInBytes;
					ApplyHeapOffset(dst.postFX.HDRCompositeSurface);
					return HDRCompositeSurfaceAllocation.SizeInBytes;
				}
			}

			namespace Overlapped
			{
				enum
				{
					BokehLayout_HDRInputSurface,
					BokehLayout_DOFOpacityBuffer,
					BokehLayout_COCBuffer,
					BokehLayout_DilatedCOCBuffer,
					BokehLayout_HalfresDOFSurface,
					BokehLayout_BuffersCount
				};

				enum
				{
					LumLayout_BloomUpChain,
					LumLayout_BloomDownChain,
					LumLayout_LDRSurface,
					LumLayout_BuffersCount
				};

				// DOF & lens flare (and currently lum adaptation with 'HDRInputSurface' for GPUs without typed UAV loads)
				template<typename LifetimeRanges>
				static inline UINT64 PostFX_1(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRanges &dst, UINT64 baseOffset)
				{
					D3D12_RESOURCE_DESC bokehBuffersDesc[BokehLayout_BuffersCount];
					bokehBuffersDesc[BokehLayout_HDRInputSurface] = offscreenBuffersDesc.HDRInputSurface();
					bokehBuffersDesc[BokehLayout_DOFOpacityBuffer] = offscreenBuffersDesc.DOFOpacityBuffer();
					bokehBuffersDesc[BokehLayout_COCBuffer] = offscreenBuffersDesc.COCBuffer();
					bokehBuffersDesc[BokehLayout_DilatedCOCBuffer] = offscreenBuffersDesc.DilatedCOCBuffer();
					bokehBuffersDesc[BokehLayout_HalfresDOFSurface] = offscreenBuffersDesc.HalfresDOFSurface();
					D3D12_RESOURCE_ALLOCATION_INFO1 bokehSuballocation[BokehLayout_BuffersCount];
					const D3D12_RESOURCE_ALLOCATION_INFO bokehAllocation = device->GetResourceAllocationInfo1(0, BokehLayout_BuffersCount, bokehBuffersDesc, bokehSuballocation);
					const auto alignedBokehOffset = AlignSize(baseOffset, bokehAllocation.Alignment);
					const auto bokehPadding = alignedBokehOffset - baseOffset;
					dst.world_postFX_1.HDRInputSurface.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_HDRInputSurface].Offset;
					dst.world_postFX_1.HDRInputSurface.size = bokehSuballocation[BokehLayout_HDRInputSurface].SizeInBytes;
					ApplyHeapOffset(dst.world_postFX_1.HDRInputSurface);
					dst.postFX_1.DOFOpacityBuffer.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_DOFOpacityBuffer].Offset;
					dst.postFX_1.DOFOpacityBuffer.size = bokehSuballocation[BokehLayout_DOFOpacityBuffer].SizeInBytes;
					ApplyHeapOffset(dst.postFX_1.DOFOpacityBuffer);
					dst.postFX_1.COCBuffer.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_COCBuffer].Offset;
					dst.postFX_1.COCBuffer.size = bokehSuballocation[BokehLayout_COCBuffer].SizeInBytes;
					ApplyHeapOffset(dst.postFX_1.COCBuffer);
					dst.postFX_1.dilatedCOCBuffer.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_DilatedCOCBuffer].Offset;
					dst.postFX_1.dilatedCOCBuffer.size = bokehSuballocation[BokehLayout_DilatedCOCBuffer].SizeInBytes;
					ApplyHeapOffset(dst.postFX_1.dilatedCOCBuffer);
					dst.postFX_1.halfresDOFSurface.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_HalfresDOFSurface].Offset;
					dst.postFX_1.halfresDOFSurface.size = bokehSuballocation[BokehLayout_HalfresDOFSurface].SizeInBytes;
					ApplyHeapOffset(dst.postFX_1.halfresDOFSurface);
					return bokehPadding + bokehAllocation.SizeInBytes;
				}

				// bloom & tonemap
				template<typename LifetimeRanges>
				static inline UINT64 PostFX_2(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRanges &dst, UINT64 baseOffset)
				{
					D3D12_RESOURCE_DESC lumBuffersDesc[LumLayout_BuffersCount];
					lumBuffersDesc[LumLayout_BloomUpChain] = offscreenBuffersDesc.BloomUpChain();
					lumBuffersDesc[LumLayout_BloomDownChain] = offscreenBuffersDesc.BloomDownChain();
					lumBuffersDesc[LumLayout_LDRSurface] = offscreenBuffersDesc.LDRSurface();
					D3D12_RESOURCE_ALLOCATION_INFO1 lumSuballocation[LumLayout_BuffersCount];
					const D3D12_RESOURCE_ALLOCATION_INFO lumAllocation = device->GetResourceAllocationInfo1(0, LumLayout_BuffersCount, lumBuffersDesc, lumSuballocation);
					const auto alignedLumOffset = AlignSize(baseOffset, lumAllocation.Alignment);
					const auto lumPadding = alignedLumOffset - baseOffset;
					dst.postFX_2.bloomUpChain.offset = alignedLumOffset + lumSuballocation[LumLayout_BloomUpChain].Offset;
					dst.postFX_2.bloomUpChain.size = lumSuballocation[LumLayout_BloomUpChain].SizeInBytes;
					ApplyHeapOffset(dst.postFX_2.bloomUpChain);
					dst.postFX_2.bloomDownChain.offset = alignedLumOffset + lumSuballocation[LumLayout_BloomDownChain].Offset;
					dst.postFX_2.bloomDownChain.size = lumSuballocation[LumLayout_BloomDownChain].SizeInBytes;
					ApplyHeapOffset(dst.postFX_2.bloomDownChain);
					dst.postFX_2.LDRSurface.offset = alignedLumOffset + lumSuballocation[LumLayout_LDRSurface].Offset;
					dst.postFX_2.LDRSurface.size = lumSuballocation[LumLayout_LDRSurface].SizeInBytes;
					ApplyHeapOffset(dst.postFX_2.LDRSurface);
					return lumPadding + lumAllocation.SizeInBytes;
				}
			}
		}
	}
}

void decltype(OffscreenBuffers::ROPs)::MarkupAllocation(const OffscreenBuffersDesc &buffersDesc, LifetimeRanges &dst)
{
	namespace Layout = OffscreenBuffersLayout::Tier_1::ROPs;

	// ZBuffer (persistent)
	const auto overlappedBaseOffset = allocationSize = Layout::Persistent::ZBuffer(buffersDesc, dst);

	// rendertarget
	const auto alignedRendertargetSize = Layout::Overlapped::Rendertarget(buffersDesc, dst, overlappedBaseOffset);

	// postFX
	const auto alignedPostFXSize = Layout::Overlapped::PostFX(buffersDesc, dst, overlappedBaseOffset);

	allocationSize += max(alignedRendertargetSize, alignedPostFXSize);
}

void decltype(OffscreenBuffers::shaders)::MarkupAllocation(const OffscreenBuffersDesc &buffersDesc, LifetimeRanges &dst)
{
	namespace Layout = OffscreenBuffersLayout::Tier_1::Shaders;

	// HDRCompositeSurface (persistent)
	const auto overlappedBaseOffset = allocationSize = Layout::Persistent::HDRCompositeSurface(buffersDesc, dst);

	// postFX 1
	const auto alignedPostFX1Size = Layout::Overlapped::PostFX_1(buffersDesc, dst, overlappedBaseOffset);

	// postFX 2
	const auto alignedPostFX2Size = Layout::Overlapped::PostFX_2(buffersDesc, dst, overlappedBaseOffset);

	allocationSize += max(alignedPostFX1Size, alignedPostFX2Size);
}

ComPtr<ID3D12Resource> OffscreenBuffers::CreateLuminanceReductionBuffer()
{
	ComPtr<ID3D12Resource> result;
	CheckHR(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(float[2048][2]), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		NULL,
		IID_PPV_ARGS(result.GetAddressOf())
	));
	NameObject(result.Get(), L"luminance reduction buffer");

	return result;
}

void OffscreenBuffers::Deleter::operator()(const OffscreenBuffers *) const
{
	store.erase(location);
	if (store.empty())
	{
		// no placed resources left anymore, release underlying VRAM heaps
		decltype(OffscreenBuffers::ROPs)::VRAMBackingStore.Reset();
		decltype(OffscreenBuffers::shaders)::VRAMBackingStore.Reset();
	}
}

OffscreenBuffers::OffscreenBuffers()
{
	// create rtv descriptor heap
	{
		const D3D12_DESCRIPTOR_HEAP_DESC desc =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			RTV_COUNT,
			D3D12_DESCRIPTOR_HEAP_FLAG_NONE
		};

		CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(rtvStore.GetAddressOf())));
	}

	// create dsv descriptor heap
	{
		const D3D12_DESCRIPTOR_HEAP_DESC desc =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			1,
			D3D12_DESCRIPTOR_HEAP_FLAG_NONE
		};

		CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(dsvStore.GetAddressOf())));
	}
}

// 1 call site
inline UINT64 OffscreenBuffers::MaxROPsAllocationSize()
{
	// TODO: use C++20 'ranges::max_element()' with projection
	return max_element(store.cbegin(), store.cend(), [](decltype(store)::const_reference left, decltype(store)::const_reference right)
	{
		return left.ROPs.allocationSize < right.ROPs.allocationSize;
	})->ROPs.allocationSize;
}

// 1 call site
inline UINT64 OffscreenBuffers::MaxShadersAllocationSize()
{
	// TODO: use C++20 'ranges::max_element()' with projection
	return max_element(store.cbegin(), store.cend(), [](decltype(store)::const_reference left, decltype(store)::const_reference right)
	{
		return left.shaders.allocationSize < right.shaders.allocationSize;
	})->shaders.allocationSize;
}

// 1 call site
inline void OffscreenBuffers::MarkupAllocation()
{
	const OffscreenBuffersDesc buffersDesc(width, height);
	ROPs.MarkupAllocation(buffersDesc, lifetimeRanges);
	shaders.MarkupAllocation(buffersDesc, lifetimeRanges);
}

// 1 call site
inline void OffscreenBuffers::DestroyBuffers()
{
	lifetimeRanges.persistent.ZBuffer.resource.Reset();
	lifetimeRanges.world_postFX_1.HDRInputSurface.resource.Reset();
	lifetimeRanges.postFX.HDRCompositeSurface.resource.Reset();
	lifetimeRanges.world.rendertarget.resource.Reset();
	lifetimeRanges.postFX_1.DOFOpacityBuffer.resource.Reset();
	lifetimeRanges.postFX_1.COCBuffer.resource.Reset();
	lifetimeRanges.postFX_1.dilatedCOCBuffer.resource.Reset();
	lifetimeRanges.postFX_1.halfresDOFSurface.resource.Reset();
	lifetimeRanges.postFX_1.DOFLayers.resource.Reset();
	lifetimeRanges.postFX_1.lensFlareSurface.resource.Reset();
	lifetimeRanges.postFX_2.bloomUpChain.resource.Reset();
	lifetimeRanges.postFX_2.bloomDownChain.resource.Reset();
	lifetimeRanges.postFX_2.LDRSurface.resource.Reset();
}

void OffscreenBuffers::ConstructBuffers()
{
	// create placed resources into shared heaps
	{
		const OffscreenBuffersDesc buffersDesc(width, height);

		// create z/stencil buffer
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.persistent.ZBuffer.offset),
			&buffersDesc.ZBuffer(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&CD3DX12_CLEAR_VALUE(Config::ZFormat::ROP, 1.f, 0xef),
			IID_PPV_ARGS(lifetimeRanges.persistent.ZBuffer.resource.ReleaseAndGetAddressOf())
		));

		// create HDR offscreen surfaces
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.world_postFX_1.HDRInputSurface.offset),
			&buffersDesc.HDRInputSurface(),
			D3D12_RESOURCE_STATE_RESOLVE_DEST,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.world_postFX_1.HDRInputSurface.resource.ReleaseAndGetAddressOf())
		));
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX.HDRCompositeSurface.offset),
			&buffersDesc.HDRCompositeSurface(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX.HDRCompositeSurface.resource.ReleaseAndGetAddressOf())
		));

		// create MSAA rendertarget
		extern const float backgroundColor[4];
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.world.rendertarget.offset),
			&buffersDesc.Rendertarget(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&CD3DX12_CLEAR_VALUE(Config::HDRFormat, backgroundColor),
			IID_PPV_ARGS(lifetimeRanges.world.rendertarget.resource.ReleaseAndGetAddressOf())
		));

		// create DOF opacity buffer
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.DOFOpacityBuffer.offset),
			&buffersDesc.DOFOpacityBuffer(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.DOFOpacityBuffer.resource.ReleaseAndGetAddressOf())
		));

		// create fullres CoC buffer
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.COCBuffer.offset),
			&buffersDesc.COCBuffer(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.COCBuffer.resource.ReleaseAndGetAddressOf())
		));

		// create halfres dilated CoC buffer
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.dilatedCOCBuffer.offset),
			&buffersDesc.DilatedCOCBuffer(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.dilatedCOCBuffer.resource.ReleaseAndGetAddressOf())
		));

		// create halfres DOF color surface
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.halfresDOFSurface.offset),
			&buffersDesc.HalfresDOFSurface(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.halfresDOFSurface.resource.ReleaseAndGetAddressOf())
		));

		// create DOF blur layers
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.DOFLayers.offset),
			&buffersDesc.DOFLayers(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.DOFLayers.resource.ReleaseAndGetAddressOf())
		));

		// create lens flare surface
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.lensFlareSurface.offset),
			&buffersDesc.LensFlareSurface(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.lensFlareSurface.resource.ReleaseAndGetAddressOf())
		));

		// create bloom chains
		{
			// up
			CheckHR(device->CreatePlacedResource(
				shaders.VRAMBackingStore.Get(),
				OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_2.bloomUpChain.offset),
				&buffersDesc.BloomUpChain(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				NULL,
				IID_PPV_ARGS(lifetimeRanges.postFX_2.bloomUpChain.resource.ReleaseAndGetAddressOf())
			));

			// down
			CheckHR(device->CreatePlacedResource(
				shaders.VRAMBackingStore.Get(),
				OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_2.bloomDownChain.offset),
				&buffersDesc.BloomDownChain(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				NULL,
				IID_PPV_ARGS(lifetimeRanges.postFX_2.bloomDownChain.resource.ReleaseAndGetAddressOf())
			));
		}

		// create LDR offscreen surface (D3D12 disallows UAV on swap chain backbuffers)
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_2.LDRSurface.offset),
			&buffersDesc.LDRSurface(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_2.LDRSurface.resource.ReleaseAndGetAddressOf())
		));
	}

	// fill descriptor heaps
	{
		const auto rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		const auto rtvStoreBase = rtvStore->GetCPUDescriptorHandleForHeapStart();

		// scene RTV
		device->CreateRenderTargetView(lifetimeRanges.world.rendertarget.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvStoreBase, SCENE_RTV, rtvSize));

		// DSV
		device->CreateDepthStencilView(lifetimeRanges.persistent.ZBuffer.resource.Get(), NULL, dsvStore->GetCPUDescriptorHandleForHeapStart());

		// DOF RTV
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc
			{
				.Format = DXGI_FORMAT_UNKNOWN,
				.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY,
				.Texture2DArray
				{
					.MipSlice = 0,
					.FirstArraySlice = 0,
					.ArraySize = 2,
					.PlaneSlice = 0
				}
			};

			// near layers
			device->CreateRenderTargetView(lifetimeRanges.postFX_1.DOFLayers.resource.Get(), &rtvDesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvStoreBase, DOF_NEAR_RTV, rtvSize));

			// far layers
			rtvDesc.Texture2DArray.FirstArraySlice = 2;
			device->CreateRenderTargetView(lifetimeRanges.postFX_1.DOFLayers.resource.Get(), &rtvDesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvStoreBase, DOF_FAR_RTV, rtvSize));
		}

		// lens flare RTV
		device->CreateRenderTargetView(lifetimeRanges.postFX_1.lensFlareSurface.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvStoreBase, LENS_FLARE_RTV, rtvSize));

		// postprocess descriptors CPU backing store
		const auto luminanceReductionTexDispatchSize = CSConfig::LuminanceReduction::TexturePass::DispatchSize({ width, height });
		postprocessCPUDescriptorTableStore.Fill(*this, luminanceReductionTexDispatchSize.x * luminanceReductionTexDispatchSize.y);
	}
}

auto OffscreenBuffers::Create(UINT width, UINT height) -> Handle
{
	Handle inserted(&store.emplace_front(), Deleter{ store.cbegin() });
	inserted->Resize(width, height, false);
	return inserted;
}

void OffscreenBuffers::Resize(UINT width, UINT height, bool allowShrink)
{
	this->width = width, this->height = height;
	MarkupAllocation();

	auto requiredROPsStore = ROPs.allocationSize, requiredShaderOnlyStore = shaders.allocationSize;
	bool refitBackingStore = !ROPs.VRAMBackingStore || !shaders.VRAMBackingStore;
	if (!refitBackingStore)
	{
		// need to refit if available backing store exhausted or significant shrinking occurs
		const auto availableROPsStore = ROPs.VRAMBackingStore->GetDesc().SizeInBytes, availableShaderOnlyStore = shaders.VRAMBackingStore->GetDesc().SizeInBytes;
		refitBackingStore = availableROPsStore < requiredROPsStore || availableShaderOnlyStore < requiredShaderOnlyStore ||
			allowShrink && availableROPsStore + availableShaderOnlyStore > (requiredROPsStore = MaxROPsAllocationSize()) + (requiredShaderOnlyStore = MaxShadersAllocationSize()) + shrinkThreshold;
	}

	if (refitBackingStore)
	{
		ROPs.VRAMBackingStore.Reset();
		shaders.VRAMBackingStore.Reset();

		// break refs to underlying heaps so that it can be destroyed before creating new heaps thus preventing VRAM usage pikes and fragmentation
		for_each(store.begin(), store.end(), mem_fn(&OffscreenBuffers::DestroyBuffers));

		CheckHR(device->CreateHeap(&CD3DX12_HEAP_DESC(requiredROPsStore, D3D12_HEAP_TYPE_DEFAULT, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES), IID_PPV_ARGS(ROPs.VRAMBackingStore.GetAddressOf())));
		CheckHR(device->CreateHeap(&CD3DX12_HEAP_DESC(requiredShaderOnlyStore, D3D12_HEAP_TYPE_DEFAULT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES), IID_PPV_ARGS(shaders.VRAMBackingStore.GetAddressOf())));

		// need to recreate all buffers due to underlying heaps have been changed
		for_each(store.begin(), store.end(), mem_fn(&OffscreenBuffers::ConstructBuffers));
	}
	else// keep other views unchanged, recreate only affected buffers
		ConstructBuffers();
}

const D3D12_CPU_DESCRIPTOR_HANDLE OffscreenBuffers::GetRTV(RTV_ID rtvID) const
{
	assert(rtvID >= 0 && rtvID < RTV_COUNT);
	const auto rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvStore->GetCPUDescriptorHandleForHeapStart(), rtvID, rtvSize);
}

const D3D12_CPU_DESCRIPTOR_HANDLE OffscreenBuffers::GetDSV() const
{
	return dsvStore->GetCPUDescriptorHandleForHeapStart();
}

ID3D12Resource *OffscreenBuffers::GetNestingBuffer(const AllocatedResource &nesting, const AllocatedResource &nested) noexcept
{
	const bool isNested = nested.offset >= nesting.offset && nested.offset + nested.size <= nesting.offset + nesting.size;
	return isNested ? nesting.resource.Get() : NULL;
}