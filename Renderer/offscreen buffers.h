#pragma once

#define NOMINMAX

#include <utility>
#include <memory>
#include <list>
#include <wrl/client.h>
#include "../tracked resource.h"
#include "../postprocess descriptor table store.h"
#include "allocator adaptors.h"

struct ID3D12Resource;
struct ID3D12Heap;
struct ID3D12DescriptorHeap;
struct D3D12_CPU_DESCRIPTOR_HANDLE;

extern void __cdecl InitRenderer();

namespace Renderer::Impl
{
	namespace WRL = Microsoft::WRL;

	class OffscreenBuffersDesc;

	class OffscreenBuffers final
	{
		template<class>
		friend class Misc::AllocatorProxyAdaptor;

	private:
		UINT width, height;

	public:
		struct AllocatedResource final
		{
			TrackedResource<ID3D12Resource> resource;
			UINT64 offset, size;
		};

	private:
		struct LifetimeRanges
		{
			struct
			{
				AllocatedResource ZBuffer;
			} persistent;

			struct
			{
				AllocatedResource HDRInputSurface;
			} world_postFX_1;

			struct
			{
				AllocatedResource HDRCompositeSurface;
			} postFX;

			struct
			{
				AllocatedResource rendertarget;
			} world;

			struct
			{
				AllocatedResource DOFOpacityBuffer, COCBuffer, dilatedCOCBuffer, halfresDOFSurface, DOFLayers, lensFlareSurface;
			} postFX_1;

			struct
			{
				AllocatedResource bloomUpChain, bloomDownChain, LDRSurface;
			} postFX_2;
		} lifetimeRanges;

		// layout for resource heap tier 1, have to separate ROPs/shaders
	private:
		struct
		{
			UINT64 allocationSize;
			static WRL::ComPtr<ID3D12Heap> VRAMBackingStore;	// don't track lifetime as it is accomplished through placed resources holding refs to underlying heap

		public:
			void MarkupAllocation(const OffscreenBuffersDesc &buffersDesc, LifetimeRanges &dst);
		} ROPs;

		struct
		{
			UINT64 allocationSize;
			static WRL::ComPtr<ID3D12Heap> VRAMBackingStore;	// don't track lifetime (similarly as for ROPs store)

		public:
			void MarkupAllocation(const OffscreenBuffersDesc &buffersDesc, LifetimeRanges &dst);
		} shaders;

	private:
		TrackedResource<ID3D12DescriptorHeap> rtvStore, dsvStore;	// is tracking really needed?
		Descriptors::PostprocessDescriptorTableStore postprocessCPUDescriptorTableStore;

	public:
		enum RTV_ID
		{
			SCENE_RTV,
			DOF_LAYERS_RTVs,
			DOF_NEAR_RTV = DOF_LAYERS_RTVs,
			DOF_FAR_RTV,
			LENS_FLARE_RTV,
			RTV_COUNT
		};

	private:
		friend extern void __cdecl ::InitRenderer();
		static WRL::ComPtr<ID3D12Resource> luminanceReductionBuffer, CreateLuminanceReductionBuffer();

	private:
		static std::list<OffscreenBuffers, Misc::AllocatorProxy<OffscreenBuffers>> store;

	private:
		class Deleter final
		{
			decltype(store)::const_iterator location;

		public:
			Deleter() = default;
			explicit Deleter(decltype(location) location) : location(location) {}

		public:
			void operator ()(const OffscreenBuffers *) const;
		};

	private:
		explicit OffscreenBuffers();
		~OffscreenBuffers() = default;
		OffscreenBuffers(OffscreenBuffers &) = delete;
		void operator =(OffscreenBuffers &) = delete;

	private:
		static UINT64 MaxROPsAllocationSize(), MaxShadersAllocationSize();
		void MarkupAllocation();
		void DestroyBuffers();
		void ConstructBuffers();

	public:
		typedef std::unique_ptr<OffscreenBuffers, Deleter> Handle;
		static Handle Create(UINT width, UINT height);

	public:
		void Resize(UINT width, UINT height, bool allowShrinkBackingStore);

	public:
		const auto &GetPersistentBuffers() const noexcept { return lifetimeRanges.persistent; }
		const auto &GetWorldAndPostFX1Buffers() const noexcept { return lifetimeRanges.world_postFX_1; }
		const auto &GetPostFXBuffers() const noexcept { return lifetimeRanges.postFX; }
		const auto &GetWorldBuffers() const noexcept { return lifetimeRanges.world; }
		const auto &GetPostFX1Buffers() const noexcept { return lifetimeRanges.postFX_1; }
		const auto &GetPostFX2Buffers() const noexcept { return lifetimeRanges.postFX_2; }
#if 0
		// fatal error LNK1179 : invalid or corrupt file : duplicate COMDAT
		template<AllocatedResource decltype(LifetimeRanges::postFX)::*nested>
		ID3D12Resource *GetNestingBuffer() const noexcept { return GetNestingBuffer(lifetimeRanges.world.rendertarget, lifetimeRanges.postFX.*nested); }
		template<AllocatedResource decltype(LifetimeRanges::postFX_1)::*nested>
		ID3D12Resource *GetNestingBuffer() const noexcept { return GetNestingBuffer(lifetimeRanges.world.rendertarget, lifetimeRanges.postFX_1.*nested); }
#else
		ID3D12Resource *GetNestingBuffer(AllocatedResource decltype(LifetimeRanges::postFX)::*nested) const noexcept { return GetNestingBuffer(lifetimeRanges.world.rendertarget, lifetimeRanges.postFX.*nested); }
		ID3D12Resource *GetNestingBuffer(AllocatedResource decltype(LifetimeRanges::postFX_1)::*nested) const noexcept { return GetNestingBuffer(lifetimeRanges.world.rendertarget, lifetimeRanges.postFX_1.*nested); }
#endif
		const D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(RTV_ID rtvID) const, GetDSV() const;
		const Descriptors::PostprocessDescriptorTableStore &GetPostprocessCPUDescriptorTableStore() const noexcept { return postprocessCPUDescriptorTableStore; }
		static const WRL::ComPtr<ID3D12Resource> &GetLuminanceReductionBuffer() noexcept { return luminanceReductionBuffer; }

	private:
		static ID3D12Resource *GetNestingBuffer(const AllocatedResource &nesting, const AllocatedResource &nested) noexcept;
	};
}