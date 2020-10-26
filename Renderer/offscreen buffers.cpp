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
void NameObject(ID3D12Object *object, LPCWSTR name) noexcept, NameObjectF(ID3D12Object *object, LPCWSTR format, ...) noexcept;

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

	namespace
	{
		template<typename Layout, underlying_type_t<Layout> resCount, class Parent>
		class ResourceDescsBuffer
		{
			D3D12_RESOURCE_DESC resDescs[resCount];

		private:
			typedef underlying_type_t<Layout> IDX;

		private:
			template<IDX ...idx>
			ResourceDescsBuffer(const OffscreenBuffersDesc &descsProvider, integer_sequence<IDX, idx...>) noexcept :
				resDescs{ (descsProvider.*Parent::template LayoutDescMapping<Layout(idx)>)()... } {}

		public:
			ResourceDescsBuffer(const OffscreenBuffersDesc &descsProvider) noexcept :
				ResourceDescsBuffer(descsProvider, make_integer_sequence<IDX, resCount>{}) {}

		public:
			operator const decltype(resDescs) &() const noexcept{ return resDescs; }
		};
	}

	template<auto GetX>
	using LifetimeRange = remove_cvref_t<invoke_result_t<decltype(GetX), OffscreenBuffers>>;

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
				static inline UINT64 ZBuffer(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRange<&OffscreenBuffers::GetPersistentBuffers> &persistent)
				{
					const D3D12_RESOURCE_ALLOCATION_INFO ZBufferAllocation = device->GetResourceAllocationInfo(0, 1, &offscreenBuffersDesc.ZBuffer());
					persistent.ZBuffer.offset = 0;
					persistent.ZBuffer.size = ZBufferAllocation.SizeInBytes;
					ApplyHeapOffset(persistent.ZBuffer);
					return ZBufferAllocation.SizeInBytes;
				}
			}

			namespace Overlapped
			{
				enum PostFXLayout
				{
					PostFXLayout_DOFLayers,
					PostFXLayout_LensFlareSurface,
					PostFXLayout_BuffersCount
				};

				class PostFXResDescsBuffer final : public ResourceDescsBuffer<PostFXLayout, PostFXLayout_BuffersCount, PostFXResDescsBuffer>
				{
					friend class ResourceDescsBuffer;

				private:
					template<PostFXLayout>
					static constexpr D3D12_RESOURCE_DESC (OffscreenBuffersDesc::*LayoutDescMapping)() const noexcept;

					template<>
					static constexpr D3D12_RESOURCE_DESC (OffscreenBuffersDesc::*LayoutDescMapping<PostFXLayout_DOFLayers>)() const noexcept = &OffscreenBuffersDesc::DOFLayers;

					template<>
					static constexpr D3D12_RESOURCE_DESC (OffscreenBuffersDesc::*LayoutDescMapping<PostFXLayout_LensFlareSurface>)() const noexcept = &OffscreenBuffersDesc::LensFlareSurface;

				public:
					using ResourceDescsBuffer::ResourceDescsBuffer;
				};

				static inline UINT64 Rendertarget(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRange<&OffscreenBuffers::GetWorldBuffers> &world, UINT64 baseOffset)
				{
					const D3D12_RESOURCE_ALLOCATION_INFO rendertargetAllocation = device->GetResourceAllocationInfo(0, 1, &offscreenBuffersDesc.Rendertarget());
					world.rendertarget.offset = AlignSize(baseOffset, rendertargetAllocation.Alignment);
					world.rendertarget.size = rendertargetAllocation.SizeInBytes;
					const auto rendertargetPadding = world.rendertarget.offset/*aligned*/ - baseOffset;
					ApplyHeapOffset(world.rendertarget);
					return rendertargetPadding + rendertargetAllocation.SizeInBytes;
				}

				static inline UINT64 PostFX(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRange<&OffscreenBuffers::GetPostFX1Buffers> &postFX_1, UINT64 baseOffset)
				{
					const PostFXResDescsBuffer postFXBuffersDesc(offscreenBuffersDesc);
					D3D12_RESOURCE_ALLOCATION_INFO1 postFXSuballocation[PostFXLayout_BuffersCount];
					const D3D12_RESOURCE_ALLOCATION_INFO postFXAllocation = device->GetResourceAllocationInfo1(0, PostFXLayout_BuffersCount, postFXBuffersDesc, postFXSuballocation);
					const auto alignedPostFXOffset = AlignSize(baseOffset, postFXAllocation.Alignment);
					const auto postFXPadding = alignedPostFXOffset - baseOffset;
					postFX_1.DOFLayers.offset = alignedPostFXOffset + postFXSuballocation[PostFXLayout_DOFLayers].Offset;
					postFX_1.DOFLayers.size = postFXSuballocation[PostFXLayout_DOFLayers].SizeInBytes;
					ApplyHeapOffset(postFX_1.DOFLayers);
					postFX_1.lensFlareSurface.offset = alignedPostFXOffset + postFXSuballocation[PostFXLayout_LensFlareSurface].Offset;
					postFX_1.lensFlareSurface.size = postFXSuballocation[PostFXLayout_LensFlareSurface].SizeInBytes;
					ApplyHeapOffset(postFX_1.lensFlareSurface);
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
				static inline UINT64 HDRCompositeSurface(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRange<&OffscreenBuffers::GetPostFXBuffers> &postFX)
				{
					const D3D12_RESOURCE_ALLOCATION_INFO HDRCompositeSurfaceAllocation = device->GetResourceAllocationInfo(0, 1, &offscreenBuffersDesc.HDRCompositeSurface());
					postFX.HDRCompositeSurface.offset = 0;
					postFX.HDRCompositeSurface.size = HDRCompositeSurfaceAllocation.SizeInBytes;
					ApplyHeapOffset(postFX.HDRCompositeSurface);
					return HDRCompositeSurfaceAllocation.SizeInBytes;
				}
			}

			namespace Overlapped
			{
				enum PostFX1Layout
				{
					PostFX1Layout_HDRInputSurface,
					PostFX1Layout_DOFOpacityBuffer,
					PostFX1Layout_COCBuffer,
					PostFX1Layout_DilatedCOCBuffer,
					PostFX1Layout_HalfresDOFSurface,
					PostFX1Layout_BuffersCount
				};

				enum PostFX2Layout
				{
					PostFX2Layout_BloomUpChain,
					PostFX2Layout_BloomDownChain,
					PostFX2Layout_LDRSurface,
					PostFX2Layout_BuffersCount
				};

				class PostFX1ResDescsBuffer final : public ResourceDescsBuffer<PostFX1Layout, PostFX1Layout_BuffersCount, PostFX1ResDescsBuffer>
				{
					friend class ResourceDescsBuffer;

				private:
					template<PostFX1Layout>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping)() const noexcept;

					template<>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping<PostFX1Layout_HDRInputSurface>)() const noexcept = &OffscreenBuffersDesc::HDRInputSurface;

					template<>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping<PostFX1Layout_DOFOpacityBuffer>)() const noexcept = &OffscreenBuffersDesc::DOFOpacityBuffer;

					template<>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping<PostFX1Layout_COCBuffer>)() const noexcept = &OffscreenBuffersDesc::COCBuffer;

					template<>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping<PostFX1Layout_DilatedCOCBuffer>)() const noexcept = &OffscreenBuffersDesc::DilatedCOCBuffer;

					template<>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping<PostFX1Layout_HalfresDOFSurface>)() const noexcept = &OffscreenBuffersDesc::HalfresDOFSurface;

				public:
					using ResourceDescsBuffer::ResourceDescsBuffer;
				};

				class PostFX2ResDescsBuffer final : public ResourceDescsBuffer<PostFX2Layout, PostFX2Layout_BuffersCount, PostFX2ResDescsBuffer>
				{
					friend class ResourceDescsBuffer;

				private:
					template<PostFX2Layout>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping)() const noexcept;

					template<>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping<PostFX2Layout_BloomUpChain>)() const noexcept = &OffscreenBuffersDesc::BloomUpChain;

					template<>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping<PostFX2Layout_BloomDownChain>)() const noexcept = &OffscreenBuffersDesc::BloomDownChain;

					template<>
					static constexpr D3D12_RESOURCE_DESC(OffscreenBuffersDesc::*LayoutDescMapping<PostFX2Layout_LDRSurface>)() const noexcept = &OffscreenBuffersDesc::LDRSurface;

				public:
					using ResourceDescsBuffer::ResourceDescsBuffer;
				};

				// DOF & lens flare (and currently lum adaptation with 'HDRInputSurface' for GPUs without typed UAV loads)
				static inline UINT64 PostFX_1(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRange<&OffscreenBuffers::GetWorldAndPostFX1Buffers> &world_postFX_1, LifetimeRange<&OffscreenBuffers::GetPostFX1Buffers> &postFX_1, UINT64 baseOffset)
				{
					const PostFX1ResDescsBuffer postFX1BuffersDesc(offscreenBuffersDesc);
					D3D12_RESOURCE_ALLOCATION_INFO1 postFX1Suballocation[PostFX1Layout_BuffersCount];
					const D3D12_RESOURCE_ALLOCATION_INFO postFX1Allocation = device->GetResourceAllocationInfo1(0, PostFX1Layout_BuffersCount, postFX1BuffersDesc, postFX1Suballocation);
					const auto alignedBokehOffset = AlignSize(baseOffset, postFX1Allocation.Alignment);
					const auto postFX1Padding = alignedBokehOffset - baseOffset;
					world_postFX_1.HDRInputSurface.offset = alignedBokehOffset + postFX1Suballocation[PostFX1Layout_HDRInputSurface].Offset;
					world_postFX_1.HDRInputSurface.size = postFX1Suballocation[PostFX1Layout_HDRInputSurface].SizeInBytes;
					ApplyHeapOffset(world_postFX_1.HDRInputSurface);
					postFX_1.DOFOpacityBuffer.offset = alignedBokehOffset + postFX1Suballocation[PostFX1Layout_DOFOpacityBuffer].Offset;
					postFX_1.DOFOpacityBuffer.size = postFX1Suballocation[PostFX1Layout_DOFOpacityBuffer].SizeInBytes;
					ApplyHeapOffset(postFX_1.DOFOpacityBuffer);
					postFX_1.COCBuffer.offset = alignedBokehOffset + postFX1Suballocation[PostFX1Layout_COCBuffer].Offset;
					postFX_1.COCBuffer.size = postFX1Suballocation[PostFX1Layout_COCBuffer].SizeInBytes;
					ApplyHeapOffset(postFX_1.COCBuffer);
					postFX_1.dilatedCOCBuffer.offset = alignedBokehOffset + postFX1Suballocation[PostFX1Layout_DilatedCOCBuffer].Offset;
					postFX_1.dilatedCOCBuffer.size = postFX1Suballocation[PostFX1Layout_DilatedCOCBuffer].SizeInBytes;
					ApplyHeapOffset(postFX_1.dilatedCOCBuffer);
					postFX_1.halfresDOFSurface.offset = alignedBokehOffset + postFX1Suballocation[PostFX1Layout_HalfresDOFSurface].Offset;
					postFX_1.halfresDOFSurface.size = postFX1Suballocation[PostFX1Layout_HalfresDOFSurface].SizeInBytes;
					ApplyHeapOffset(postFX_1.halfresDOFSurface);
					return postFX1Padding + postFX1Allocation.SizeInBytes;
				}

				// bloom & tonemap
				static inline UINT64 PostFX_2(const OffscreenBuffersDesc &offscreenBuffersDesc, LifetimeRange<&OffscreenBuffers::GetPostFX2Buffers> &postFX_2, UINT64 baseOffset)
				{
					const PostFX2ResDescsBuffer postFX2BuffersDesc(offscreenBuffersDesc);
					D3D12_RESOURCE_ALLOCATION_INFO1 postFX2Suballocation[PostFX2Layout_BuffersCount];
					const D3D12_RESOURCE_ALLOCATION_INFO postFX2Allocation = device->GetResourceAllocationInfo1(0, PostFX2Layout_BuffersCount, postFX2BuffersDesc, postFX2Suballocation);
					const auto alignedLumOffset = AlignSize(baseOffset, postFX2Allocation.Alignment);
					const auto postFX2Padding = alignedLumOffset - baseOffset;
					postFX_2.bloomUpChain.offset = alignedLumOffset + postFX2Suballocation[PostFX2Layout_BloomUpChain].Offset;
					postFX_2.bloomUpChain.size = postFX2Suballocation[PostFX2Layout_BloomUpChain].SizeInBytes;
					ApplyHeapOffset(postFX_2.bloomUpChain);
					postFX_2.bloomDownChain.offset = alignedLumOffset + postFX2Suballocation[PostFX2Layout_BloomDownChain].Offset;
					postFX_2.bloomDownChain.size = postFX2Suballocation[PostFX2Layout_BloomDownChain].SizeInBytes;
					ApplyHeapOffset(postFX_2.bloomDownChain);
					postFX_2.LDRSurface.offset = alignedLumOffset + postFX2Suballocation[PostFX2Layout_LDRSurface].Offset;
					postFX_2.LDRSurface.size = postFX2Suballocation[PostFX2Layout_LDRSurface].SizeInBytes;
					ApplyHeapOffset(postFX_2.LDRSurface);
					return postFX2Padding + postFX2Allocation.SizeInBytes;
				}
			}
		}
	}
}

void decltype(OffscreenBuffers::ROPs)::MarkupAllocation(const OffscreenBuffersDesc &buffersDesc, LifetimeRanges &dst)
{
	namespace Layout = OffscreenBuffersLayout::Tier_1::ROPs;

	// ZBuffer (persistent)
	const auto overlappedBaseOffset = allocationSize = Layout::Persistent::ZBuffer(buffersDesc, dst.persistent);

	// rendertarget
	const auto alignedRendertargetSize = Layout::Overlapped::Rendertarget(buffersDesc, dst.world, overlappedBaseOffset);

	// postFX
	const auto alignedPostFXSize = Layout::Overlapped::PostFX(buffersDesc, dst.postFX_1, overlappedBaseOffset);

	allocationSize += max(alignedRendertargetSize, alignedPostFXSize);
}

void decltype(OffscreenBuffers::shaders)::MarkupAllocation(const OffscreenBuffersDesc &buffersDesc, LifetimeRanges &dst)
{
	namespace Layout = OffscreenBuffersLayout::Tier_1::Shaders;

	// HDRCompositeSurface (persistent)
	const auto overlappedBaseOffset = allocationSize = Layout::Persistent::HDRCompositeSurface(buffersDesc, dst.postFX);

	// postFX 1
	const auto alignedPostFX1Size = Layout::Overlapped::PostFX_1(buffersDesc, dst.world_postFX_1, dst.postFX_1, overlappedBaseOffset);

	// postFX 2
	const auto alignedPostFX2Size = Layout::Overlapped::PostFX_2(buffersDesc, dst.postFX_2, overlappedBaseOffset);

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
		NameObjectF(rtvStore.Get(), L"RTV store (offscreen buffers object %p)", this);
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
		NameObjectF(dsvStore.Get(), L"DSV store (offscreen buffers object %p)", this);
	}
}

// 1 call site
inline pair<UINT64, UINT64> OffscreenBuffers::MaxAllocationSize()
{
	// or 'transform_reduce'
	return accumulate(store.cbegin(), store.cend(), make_pair(UINT64_C(0), UINT64_C(0)), [](const pair<UINT64, UINT64> &left, decltype(store)::const_reference right)
	{
		return make_pair(max(left.first, right.ROPs.allocationSize), max(left.second, right.shaders.allocationSize));
	});
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
		version++;	// preincrement improves exception safety

		// create z/stencil buffer
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.persistent.ZBuffer.offset),
			&buffersDesc.ZBuffer(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&CD3DX12_CLEAR_VALUE(Config::ZFormat::ROP, 1.f, 0xef),
			IID_PPV_ARGS(lifetimeRanges.persistent.ZBuffer.resource.ReleaseAndGetAddressOf())
		));
		NameObjectF(lifetimeRanges.persistent.ZBuffer.Resource(), L"Z buffer [%lu] (offscreen buffers object %p)", version, this);

		// create HDR offscreen surfaces
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.world_postFX_1.HDRInputSurface.offset),
			&buffersDesc.HDRInputSurface(),
			D3D12_RESOURCE_STATE_RESOLVE_DEST,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.world_postFX_1.HDRInputSurface.resource.ReleaseAndGetAddressOf())
		));
		NameObjectF(lifetimeRanges.world_postFX_1.HDRInputSurface.Resource(), L"HDR input surface [%lu] (offscreen buffers object %p)", version, this);
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX.HDRCompositeSurface.offset),
			&buffersDesc.HDRCompositeSurface(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX.HDRCompositeSurface.resource.ReleaseAndGetAddressOf())
		));
		NameObjectF(lifetimeRanges.postFX.HDRCompositeSurface.Resource(), L"HDR composite surface [%lu] (offscreen buffers object %p)", version, this);

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
		NameObjectF(lifetimeRanges.world.rendertarget.Resource(), L"MSAA rendertarget [%lu] (offscreen buffers object %p)", version, this);

		// create DOF opacity buffer
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.DOFOpacityBuffer.offset),
			&buffersDesc.DOFOpacityBuffer(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.DOFOpacityBuffer.resource.ReleaseAndGetAddressOf())
		));
		NameObjectF(lifetimeRanges.postFX_1.DOFOpacityBuffer.Resource(), L"DOF opacity buffer [%lu] (offscreen buffers object %p)", version, this);

		// create fullres CoC buffer
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.COCBuffer.offset),
			&buffersDesc.COCBuffer(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.COCBuffer.resource.ReleaseAndGetAddressOf())
		));
		NameObjectF(lifetimeRanges.postFX_1.COCBuffer.Resource(), L"fullres CoC buffer [%lu] (offscreen buffers object %p)", version, this);

		// create halfres dilated CoC buffer
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.dilatedCOCBuffer.offset),
			&buffersDesc.DilatedCOCBuffer(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.dilatedCOCBuffer.resource.ReleaseAndGetAddressOf())
		));
		NameObjectF(lifetimeRanges.postFX_1.dilatedCOCBuffer.Resource(), L"halfres dilated CoC buffer [%lu] (offscreen buffers object %p)", version, this);

		// create halfres DOF color surface
		CheckHR(device->CreatePlacedResource(
			shaders.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.halfresDOFSurface.offset),
			&buffersDesc.HalfresDOFSurface(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.halfresDOFSurface.resource.ReleaseAndGetAddressOf())
		));
		NameObjectF(lifetimeRanges.postFX_1.halfresDOFSurface.Resource(), L"halfres DOF color surface [%lu] (offscreen buffers object %p)", version, this);

		// create DOF blur layers
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.DOFLayers.offset),
			&buffersDesc.DOFLayers(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.DOFLayers.resource.ReleaseAndGetAddressOf())
		));
		NameObjectF(lifetimeRanges.postFX_1.DOFLayers.Resource(), L"DOF blur layers [%lu] (offscreen buffers object %p)", version, this);

		// create lens flare surface
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_1.lensFlareSurface.offset),
			&buffersDesc.LensFlareSurface(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			NULL,
			IID_PPV_ARGS(lifetimeRanges.postFX_1.lensFlareSurface.resource.ReleaseAndGetAddressOf())
		));
		NameObjectF(lifetimeRanges.postFX_1.lensFlareSurface.Resource(), L"lens flare surface [%lu] (offscreen buffers object %p)", version, this);

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
			NameObjectF(lifetimeRanges.postFX_2.bloomUpChain.Resource(), L"bloom up chain [%lu] (offscreen buffers object %p)", version, this);

			// down
			CheckHR(device->CreatePlacedResource(
				shaders.VRAMBackingStore.Get(),
				OffscreenBuffersLayout::RemoveHeapOffset(lifetimeRanges.postFX_2.bloomDownChain.offset),
				&buffersDesc.BloomDownChain(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				NULL,
				IID_PPV_ARGS(lifetimeRanges.postFX_2.bloomDownChain.resource.ReleaseAndGetAddressOf())
			));
			NameObjectF(lifetimeRanges.postFX_2.bloomDownChain.Resource(), L"bloom down chain [%lu] (offscreen buffers object %p)", version, this);
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
		NameObjectF(lifetimeRanges.postFX_2.LDRSurface.Resource(), L"LDR offscreen surface [%lu] (offscreen buffers object %p)", version, this);
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

	auto requiredROPsStore = ROPs.allocationSize, requiredShadersStore = shaders.allocationSize;
	bool refitBackingStore = !ROPs.VRAMBackingStore || !shaders.VRAMBackingStore;
	if (!refitBackingStore)
	{
		// need to refit if available backing store exhausted or significant shrinking occurs
		const auto availableROPsStore = ROPs.VRAMBackingStore->GetDesc().SizeInBytes, availableShadersStore = shaders.VRAMBackingStore->GetDesc().SizeInBytes;
		refitBackingStore = availableROPsStore < requiredROPsStore || availableShadersStore < requiredShadersStore ||
			allowShrink && availableROPsStore + availableShadersStore > (tie(requiredROPsStore, requiredShadersStore) = MaxAllocationSize(), requiredROPsStore + requiredShadersStore) + shrinkThreshold;
	}

	if (refitBackingStore)
	{
		ROPs.VRAMBackingStore.Reset();
		shaders.VRAMBackingStore.Reset();

		// break refs to underlying heaps so that it can be destroyed before creating new heaps thus preventing VRAM usage pikes and fragmentation
		for_each(store.begin(), store.end(), mem_fn(&OffscreenBuffers::DestroyBuffers));

		static unsigned long version = -1;
		version++;	// preincrement improves exception safety
		CheckHR(device->CreateHeap(&CD3DX12_HEAP_DESC(requiredROPsStore, D3D12_HEAP_TYPE_DEFAULT, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES), IID_PPV_ARGS(ROPs.VRAMBackingStore.GetAddressOf())));
		NameObjectF(ROPs.VRAMBackingStore.Get(), L"VRAM backing store for ROPs offscreen buffers [%lu]", version);
		CheckHR(device->CreateHeap(&CD3DX12_HEAP_DESC(requiredShadersStore, D3D12_HEAP_TYPE_DEFAULT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES), IID_PPV_ARGS(shaders.VRAMBackingStore.GetAddressOf())));
		NameObjectF(shaders.VRAMBackingStore.Get(), L"VRAM backing store for shaders offscreen buffers [%lu]", version);

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