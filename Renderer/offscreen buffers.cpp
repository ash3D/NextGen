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

namespace
{
	namespace Config = Renderer::Config;

	class ResourceDesc final
	{
		Math::VectorMath::HLSL::uint2 fullres, halfres;
		unsigned long bloomUpChainLen, bloomDownChainLen;
		DXGI_SAMPLE_DESC MSAA_mode = Config::MSAA();

	public:
		explicit ResourceDesc(UINT width, UINT height);

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

	ResourceDesc::ResourceDesc(UINT width, UINT height) : fullres(width, height), halfres((fullres + 1) / 2)
	{
		// TODO: replace with C++20 std::log2p1 with halfres
		_BitScanReverse(&bloomUpChainLen, max(width, height));
		bloomUpChainLen = min(bloomUpChainLen, 6ul);
		bloomDownChainLen = bloomUpChainLen - 1;
	}
}

decltype(OffscreenBuffers::store) OffscreenBuffers::store;

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

		CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(rtvHeap.GetAddressOf())));
	}

	// create dsv descriptor heap
	{
		const D3D12_DESCRIPTOR_HEAP_DESC desc =
		{
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			1,
			D3D12_DESCRIPTOR_HEAP_FLAG_NONE
		};

		CheckHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(dsvHeap.GetAddressOf())));
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
inline UINT64 OffscreenBuffers::MaxShaderOnlyAllocationSize()
{
	// TODO: use C++20 'ranges::max_element()' with projection
	return max_element(store.cbegin(), store.cend(), [](decltype(store)::const_reference left, decltype(store)::const_reference right)
	{
		return left.shaderOnly.allocationSize < right.shaderOnly.allocationSize;
	})->shaderOnly.allocationSize;
}

// 1 call site
inline void OffscreenBuffers::MarkupAllocation()
{
	const ResourceDesc resourceDesc(width, height);

	// ROPs
	{
		// ZBuffer (persistent)
		const D3D12_RESOURCE_ALLOCATION_INFO ZBufferAllocation = device->GetResourceAllocationInfo(0, 1, &resourceDesc.ZBuffer());
		ROPs.persistent.ZBuffer.offset = 0;
		ROPs.allocationSize = ROPs.persistent.ZBuffer.size = ZBufferAllocation.SizeInBytes;

		// rendertarget
		const D3D12_RESOURCE_ALLOCATION_INFO rendertargetAllocation = device->GetResourceAllocationInfo(0, 1, &resourceDesc.Rendertarget());
		ROPs.overlapped.rendertarget.offset = AlignSize(ZBufferAllocation.SizeInBytes, rendertargetAllocation.Alignment);
		ROPs.overlapped.rendertarget.size = rendertargetAllocation.SizeInBytes;
		const auto rendertargetPadding = ROPs.overlapped.rendertarget.offset/*aligned*/ - ZBufferAllocation.SizeInBytes/*base*/;

		// postFX
		enum
		{
			PostFXLayout_DOFLayers,
			PostFXLayout_LensFlareSurface,
			PostFXLayout_BuffersCount
		};
		D3D12_RESOURCE_DESC postFXBuffersDesc[PostFXLayout_BuffersCount];
		postFXBuffersDesc[PostFXLayout_DOFLayers] = resourceDesc.DOFLayers();
		postFXBuffersDesc[PostFXLayout_LensFlareSurface] = resourceDesc.LensFlareSurface();
		D3D12_RESOURCE_ALLOCATION_INFO1 postFXSuballocation[PostFXLayout_BuffersCount];
		const D3D12_RESOURCE_ALLOCATION_INFO postFXAllocation = device->GetResourceAllocationInfo1(0, PostFXLayout_BuffersCount, postFXBuffersDesc, postFXSuballocation);
		const auto alignedPostFXOffset = AlignSize(ZBufferAllocation.SizeInBytes, postFXAllocation.Alignment);
		const auto postFXPadding = alignedPostFXOffset - ZBufferAllocation.SizeInBytes/*base*/;
		ROPs.overlapped.postFX.DOFLayers.offset = alignedPostFXOffset + postFXSuballocation[PostFXLayout_DOFLayers].Offset;
		ROPs.overlapped.postFX.DOFLayers.size = postFXSuballocation[PostFXLayout_DOFLayers].SizeInBytes;
		ROPs.overlapped.postFX.lensFlareSurface.offset = alignedPostFXOffset + postFXSuballocation[PostFXLayout_LensFlareSurface].Offset;
		ROPs.overlapped.postFX.lensFlareSurface.size = postFXSuballocation[PostFXLayout_LensFlareSurface].SizeInBytes;

		ROPs.allocationSize += max(rendertargetPadding + rendertargetAllocation.SizeInBytes, postFXPadding + postFXAllocation.SizeInBytes);
	}

	// shaderOnly
	{
		// HDRCompositeSurface (persistent)
		const D3D12_RESOURCE_ALLOCATION_INFO HDRCompositeSurfaceAllocation = device->GetResourceAllocationInfo(0, 1, &resourceDesc.HDRCompositeSurface());
		shaderOnly.persistent.HDRCompositeSurface.offset = 0;
		shaderOnly.allocationSize = shaderOnly.persistent.HDRCompositeSurface.size = HDRCompositeSurfaceAllocation.SizeInBytes;

		// bokeh
		enum
		{
			BokehLayout_HDRInputSurface,
			BokehLayout_DOFOpacityBuffer,
			BokehLayout_COCBuffer,
			BokehLayout_DilatedCOCBuffer,
			BokehLayout_HalfresDOFSurface,
			BokehLayout_BuffersCount
		};
		D3D12_RESOURCE_DESC bokehBuffersDesc[BokehLayout_BuffersCount];
		bokehBuffersDesc[BokehLayout_HDRInputSurface] = resourceDesc.HDRInputSurface();
		bokehBuffersDesc[BokehLayout_DOFOpacityBuffer] = resourceDesc.DOFOpacityBuffer();
		bokehBuffersDesc[BokehLayout_COCBuffer] = resourceDesc.COCBuffer();
		bokehBuffersDesc[BokehLayout_DilatedCOCBuffer] = resourceDesc.DilatedCOCBuffer();
		bokehBuffersDesc[BokehLayout_HalfresDOFSurface] = resourceDesc.HalfresDOFSurface();
		D3D12_RESOURCE_ALLOCATION_INFO1 bokehSuballocation[BokehLayout_BuffersCount];
		const D3D12_RESOURCE_ALLOCATION_INFO bokehAllocation = device->GetResourceAllocationInfo1(0, BokehLayout_BuffersCount, bokehBuffersDesc, bokehSuballocation);
		const auto alignedBokehOffset = AlignSize(HDRCompositeSurfaceAllocation.SizeInBytes, bokehAllocation.Alignment);
		const auto bokehPadding = alignedBokehOffset - HDRCompositeSurfaceAllocation.SizeInBytes/*base*/;
		shaderOnly.overlapped.bokeh.HDRInputSurface.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_HDRInputSurface].Offset;
		shaderOnly.overlapped.bokeh.HDRInputSurface.size = bokehSuballocation[BokehLayout_HDRInputSurface].SizeInBytes;
		shaderOnly.overlapped.bokeh.DOFOpacityBuffer.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_DOFOpacityBuffer].Offset;
		shaderOnly.overlapped.bokeh.DOFOpacityBuffer.size = bokehSuballocation[BokehLayout_DOFOpacityBuffer].SizeInBytes;
		shaderOnly.overlapped.bokeh.COCBuffer.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_COCBuffer].Offset;
		shaderOnly.overlapped.bokeh.COCBuffer.size = bokehSuballocation[BokehLayout_COCBuffer].SizeInBytes;
		shaderOnly.overlapped.bokeh.dilatedCOCBuffer.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_DilatedCOCBuffer].Offset;
		shaderOnly.overlapped.bokeh.dilatedCOCBuffer.size = bokehSuballocation[BokehLayout_DilatedCOCBuffer].SizeInBytes;
		shaderOnly.overlapped.bokeh.halfresDOFSurface.offset = alignedBokehOffset + bokehSuballocation[BokehLayout_HalfresDOFSurface].Offset;
		shaderOnly.overlapped.bokeh.halfresDOFSurface.size = bokehSuballocation[BokehLayout_HalfresDOFSurface].SizeInBytes;

		// lum
		enum
		{
			LumLayout_BloomUpChain,
			LumLayout_BloomDownChain,
			LumLayout_LDRSurface,
			LumLayout_BuffersCount
		};
		D3D12_RESOURCE_DESC lumBuffersDesc[LumLayout_BuffersCount];
		lumBuffersDesc[LumLayout_BloomUpChain] = resourceDesc.BloomUpChain();
		lumBuffersDesc[LumLayout_BloomDownChain] = resourceDesc.BloomDownChain();
		lumBuffersDesc[LumLayout_LDRSurface] = resourceDesc.LDRSurface();
		D3D12_RESOURCE_ALLOCATION_INFO1 lumSuballocation[LumLayout_BuffersCount];
		const D3D12_RESOURCE_ALLOCATION_INFO lumAllocation = device->GetResourceAllocationInfo1(0, LumLayout_BuffersCount, lumBuffersDesc, lumSuballocation);
		const auto alignedLumOffset = AlignSize(HDRCompositeSurfaceAllocation.SizeInBytes, lumAllocation.Alignment);
		const auto lumPadding = alignedLumOffset - HDRCompositeSurfaceAllocation.SizeInBytes/*base*/;
		shaderOnly.overlapped.lum.bloomUpChain.offset = alignedLumOffset + lumSuballocation[LumLayout_BloomUpChain].Offset;
		shaderOnly.overlapped.lum.bloomUpChain.size = lumSuballocation[LumLayout_BloomUpChain].SizeInBytes;
		shaderOnly.overlapped.lum.bloomDownChain.offset = alignedLumOffset + lumSuballocation[LumLayout_BloomDownChain].Offset;
		shaderOnly.overlapped.lum.bloomDownChain.size = lumSuballocation[LumLayout_BloomDownChain].SizeInBytes;
		shaderOnly.overlapped.lum.LDRSurface.offset = alignedLumOffset + lumSuballocation[LumLayout_LDRSurface].Offset;
		shaderOnly.overlapped.lum.LDRSurface.size = lumSuballocation[LumLayout_LDRSurface].SizeInBytes;

		shaderOnly.allocationSize += max(bokehPadding + bokehAllocation.SizeInBytes, lumPadding + lumAllocation.SizeInBytes);
	}
}

// 1 call site
inline void OffscreenBuffers::DestroyBuffers()
{
	ROPs.persistent.ZBuffer.resource.Reset();
	ROPs.overlapped.rendertarget.resource.Reset();
	ROPs.overlapped.postFX.DOFLayers.resource.Reset();
	ROPs.overlapped.postFX.lensFlareSurface.resource.Reset();
	shaderOnly.persistent.HDRCompositeSurface.resource.Reset();
	shaderOnly.overlapped.bokeh.HDRInputSurface.resource.Reset();
	shaderOnly.overlapped.bokeh.DOFOpacityBuffer.resource.Reset();
	shaderOnly.overlapped.bokeh.COCBuffer.resource.Reset();
	shaderOnly.overlapped.bokeh.dilatedCOCBuffer.resource.Reset();
	shaderOnly.overlapped.bokeh.halfresDOFSurface.resource.Reset();
	shaderOnly.overlapped.lum.bloomUpChain.resource.Reset();
	shaderOnly.overlapped.lum.bloomDownChain.resource.Reset();
	shaderOnly.overlapped.lum.LDRSurface.resource.Reset();
}

// 1 call site
inline void OffscreenBuffers::ConstructBuffers()
{
	// create placed resources into shared heaps
	{
		const ResourceDesc resourceDesc(width, height);

		// create z/stencil buffer
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			ROPs.persistent.ZBuffer.offset,
			&resourceDesc.ZBuffer(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&CD3DX12_CLEAR_VALUE(Config::ZFormat::ROP, 1.f, 0xef),
			IID_PPV_ARGS(ROPs.persistent.ZBuffer.resource.GetAddressOf())
		));

		// create MSAA rendertarget
		extern const float backgroundColor[4];
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			ROPs.overlapped.rendertarget.offset,
			&resourceDesc.Rendertarget(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&CD3DX12_CLEAR_VALUE(Config::HDRFormat, backgroundColor),
			IID_PPV_ARGS(ROPs.overlapped.rendertarget.resource.GetAddressOf())
		));

		// create DOF blur layers
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			ROPs.overlapped.postFX.DOFLayers.offset,
			&resourceDesc.DOFLayers(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(ROPs.overlapped.postFX.DOFLayers.resource.GetAddressOf())
		));

		// create lens flare surface
		CheckHR(device->CreatePlacedResource(
			ROPs.VRAMBackingStore.Get(),
			ROPs.overlapped.postFX.lensFlareSurface.offset,
			&resourceDesc.LensFlareSurface(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			NULL,
			IID_PPV_ARGS(ROPs.overlapped.postFX.lensFlareSurface.resource.GetAddressOf())
		));

		// create HDR offscreen surfaces
		CheckHR(device->CreatePlacedResource(
			shaderOnly.VRAMBackingStore.Get(),
			shaderOnly.persistent.HDRCompositeSurface.offset,
			&resourceDesc.HDRCompositeSurface(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(shaderOnly.persistent.HDRCompositeSurface.resource.GetAddressOf())
		));
		CheckHR(device->CreatePlacedResource(
			shaderOnly.VRAMBackingStore.Get(),
			shaderOnly.overlapped.bokeh.HDRInputSurface.offset,
			&resourceDesc.HDRInputSurface(),
			D3D12_RESOURCE_STATE_RESOLVE_DEST,
			NULL,
			IID_PPV_ARGS(shaderOnly.overlapped.bokeh.HDRInputSurface.resource.GetAddressOf())
		));

		// create DOF opacity buffer
		CheckHR(device->CreatePlacedResource(
			shaderOnly.VRAMBackingStore.Get(),
			shaderOnly.overlapped.bokeh.DOFOpacityBuffer.offset,
			&resourceDesc.DOFOpacityBuffer(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(shaderOnly.overlapped.bokeh.DOFOpacityBuffer.resource.GetAddressOf())
		));

		// create fullres CoC buffer
		CheckHR(device->CreatePlacedResource(
			shaderOnly.VRAMBackingStore.Get(),
			shaderOnly.overlapped.bokeh.COCBuffer.offset,
			&resourceDesc.COCBuffer(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(shaderOnly.overlapped.bokeh.COCBuffer.resource.GetAddressOf())
		));

		// create halfres dilated CoC buffer
		CheckHR(device->CreatePlacedResource(
			shaderOnly.VRAMBackingStore.Get(),
			shaderOnly.overlapped.bokeh.dilatedCOCBuffer.offset,
			&resourceDesc.DilatedCOCBuffer(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(shaderOnly.overlapped.bokeh.dilatedCOCBuffer.resource.GetAddressOf())
		));

		// create halfres DOF color surface
		CheckHR(device->CreatePlacedResource(
			shaderOnly.VRAMBackingStore.Get(),
			shaderOnly.overlapped.bokeh.halfresDOFSurface.offset,
			&resourceDesc.HalfresDOFSurface(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(shaderOnly.overlapped.bokeh.halfresDOFSurface.resource.GetAddressOf())
		));

		// create bloom chains
		{
			// up
			CheckHR(device->CreatePlacedResource(
				shaderOnly.VRAMBackingStore.Get(),
				shaderOnly.overlapped.lum.bloomUpChain.offset,
				&resourceDesc.BloomUpChain(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				NULL,
				IID_PPV_ARGS(shaderOnly.overlapped.lum.bloomUpChain.resource.GetAddressOf())
			));

			// down
			CheckHR(device->CreatePlacedResource(
				shaderOnly.VRAMBackingStore.Get(),
				shaderOnly.overlapped.lum.bloomDownChain.offset,
				&resourceDesc.BloomDownChain(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				NULL,
				IID_PPV_ARGS(shaderOnly.overlapped.lum.bloomDownChain.resource.GetAddressOf())
			));
		}

		// create LDR offscreen surface (D3D12 disallows UAV on swap chain backbuffers)
		CheckHR(device->CreatePlacedResource(
			shaderOnly.VRAMBackingStore.Get(),
			shaderOnly.overlapped.lum.LDRSurface.offset,
			&resourceDesc.LDRSurface(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			NULL,
			IID_PPV_ARGS(shaderOnly.overlapped.lum.LDRSurface.resource.GetAddressOf())
		));
	}

	// fill descriptor heaps
	{
		const auto rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		const auto rtvHeapStart = rtvHeap->GetCPUDescriptorHandleForHeapStart();

		// fill scene RTV heap
		device->CreateRenderTargetView(ROPs.overlapped.rendertarget.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, SCENE_RTV, rtvSize));

		// fill DSV heap
		device->CreateDepthStencilView(ROPs.persistent.ZBuffer.resource.Get(), NULL, dsvHeap->GetCPUDescriptorHandleForHeapStart());

		// fill DOF RTV heap
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
			device->CreateRenderTargetView(ROPs.overlapped.postFX.DOFLayers.resource.Get(), &rtvDesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, DOF_NEAR_RTV, rtvSize));

			// far layers
			rtvDesc.Texture2DArray.FirstArraySlice = 2;
			device->CreateRenderTargetView(ROPs.overlapped.postFX.DOFLayers.resource.Get(), &rtvDesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, DOF_FAR_RTV, rtvSize));
		}

		// fill lens flare RTV heap
		device->CreateRenderTargetView(ROPs.overlapped.postFX.lensFlareSurface.resource.Get(), NULL, CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapStart, LENS_FLARE_RTV, rtvSize));

		// fill postprocess descriptors CPU backing store
		const auto luminanceReductionTexDispatchSize = CSConfig::LuminanceReduction::TexturePass::DispatchSize({ width, height });
		postprocessCPUDescriptorHeap.Fill(*this, luminanceReductionTexDispatchSize.x * luminanceReductionTexDispatchSize.y);
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

	auto requiredROPsStore = ROPs.allocationSize, requiredShaderOnlyStore = shaderOnly.allocationSize;
	bool refitBackingStore = !ROPs.VRAMBackingStore || !shaderOnly.VRAMBackingStore;
	if (!refitBackingStore)
	{
		// need to refit if available backing store exhausted or significant shrinking occurs
		const auto availableROPsStore = ROPs.VRAMBackingStore->GetDesc().SizeInBytes, availableShaderOnlyStore = shaderOnly.VRAMBackingStore->GetDesc().SizeInBytes;
		refitBackingStore = availableROPsStore < requiredROPsStore || availableShaderOnlyStore < requiredShaderOnlyStore ||
			allowShrink && availableROPsStore + availableShaderOnlyStore > (requiredROPsStore = MaxROPsAllocationSize()) + (requiredShaderOnlyStore = MaxShaderOnlyAllocationSize()) + shrinkThreshold;
	}

	if (refitBackingStore)
	{
		ROPs.VRAMBackingStore.Reset();
		shaderOnly.VRAMBackingStore.Reset();

		// break refs to underlying heaps so that it can be destroyed before creating new heaps thus preventing VRAM usage pikes and fragmentation
		for_each(store.begin(), store.end(), mem_fn(&OffscreenBuffers::DestroyBuffers));

		CheckHR(device->CreateHeap(&CD3DX12_HEAP_DESC(requiredROPsStore, D3D12_HEAP_TYPE_DEFAULT, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES), IID_PPV_ARGS(ROPs.VRAMBackingStore.GetAddressOf())));
		CheckHR(device->CreateHeap(&CD3DX12_HEAP_DESC(requiredShaderOnlyStore, D3D12_HEAP_TYPE_DEFAULT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES), IID_PPV_ARGS(shaderOnly.VRAMBackingStore.GetAddressOf())));
	}
		
	for_each(store.begin(), store.end(), mem_fn(&OffscreenBuffers::ConstructBuffers));
}

const D3D12_CPU_DESCRIPTOR_HANDLE OffscreenBuffers::GetRTV() const { return GetRTV(SCENE_RTV); }

const D3D12_CPU_DESCRIPTOR_HANDLE OffscreenBuffers::GetDOFLayersRTV() const { return GetRTV(DOF_LAYERS_RTVs); }

const D3D12_CPU_DESCRIPTOR_HANDLE OffscreenBuffers::GetLensFlareRTV() const { return GetRTV(LENS_FLARE_RTV); }

const D3D12_CPU_DESCRIPTOR_HANDLE OffscreenBuffers::GetDSV() const
{
	return dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

const D3D12_CPU_DESCRIPTOR_HANDLE OffscreenBuffers::GetRTV(RTV_ID rtvID) const
{
	const auto rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap->GetCPUDescriptorHandleForHeapStart(), rtvID, rtvSize);
}