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

		// layout for resource heap tier 1, have to separate ROPs/shaderOnly
	private:
		struct
		{
			friend class OffscreenBuffers;

		public:
			struct
			{
				AllocatedResource ZBuffer;
			} persistent;

			struct
			{
				AllocatedResource rendertarget;

				struct
				{
					AllocatedResource DOFLayers, lensFlareSurface;
				} postFX;
			} overlapped;

		private:
			UINT64 allocationSize;
			static WRL::ComPtr<ID3D12Heap> VRAMBackingStore;	// don't track lifetime as it is accomplished through placed resources holding refs to underlying heap
		} ROPs;

		struct
		{
			friend class OffscreenBuffers;

		public:
			struct
			{
				AllocatedResource HDRCompositeSurface;
			} persistent;

			struct
			{
				// DOF & lens flare (and currently lum adaptation with 'HDRInputSurface' for GPUs without typed UAV loads)
				struct
				{
					AllocatedResource HDRInputSurface, DOFOpacityBuffer, COCBuffer, dilatedCOCBuffer, halfresDOFSurface;
				} bokeh;

				// bloom & tonemap
				struct
				{
					AllocatedResource bloomUpChain, bloomDownChain, LDRSurface;
				} lum;
			} overlapped;

		private:
			UINT64 allocationSize;
			static WRL::ComPtr<ID3D12Heap> VRAMBackingStore;	// don't track lifetime (similarly as for ROPs store)
		} shaderOnly;

	private:
		TrackedResource<ID3D12DescriptorHeap> rtvHeap, dsvHeap;	// is tracking really needed?
		Descriptors::PostprocessDescriptorTableStore postprocessCPUDescriptorHeap;

	private:
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
		static UINT64 MaxROPsAllocationSize(), MaxShaderOnlyAllocationSize();
		void MarkupAllocation();
		void DestroyBuffers();
		void ConstructBuffers();

	public:
		typedef std::unique_ptr<OffscreenBuffers, Deleter> Handle;
		static Handle Create(UINT width, UINT height);

	public:
		void Resize(UINT width, UINT height, bool allowShrinkBackingStore);

	public:
		const auto &GetROPsBuffers() const noexcept { return ROPs; }
		const auto &GetShaderOnlyBuffers() const noexcept { return shaderOnly; }
		const D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const, GetDOFLayersRTV() const, GetLensFlareRTV() const, GetDSV() const;
		const Descriptors::PostprocessDescriptorTableStore &GetPostprocessCPUDescriptorHeap() const noexcept { return postprocessCPUDescriptorHeap; }
		static const WRL::ComPtr<ID3D12Resource> &GetLuminanceReductionBuffer() noexcept { return luminanceReductionBuffer; }

	private:
		const D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(RTV_ID rtvID) const;
	};
}