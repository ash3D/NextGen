#pragma once

#define NOMINMAX

#include <memory>
#include <memory_resource>
#include <wrl/client.h>
#include "texture.hh"
#include "../tracked resource.h"
#include "../render pipeline.h"

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12DescriptorHeap;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_GPU_DESCRIPTOR_HANDLE;

extern void __cdecl InitRenderer();

namespace Renderer
{
	namespace WRL = Microsoft::WRL;

	class Sky;

	namespace Impl
	{
		class World;

		namespace Descriptors
		{
			class PostprocessDescriptorTableStore;

			namespace GPUDescriptorHeap
			{
				struct PerFrameDescriptorTables StreamPerFrameDescriptorTables(const Renderer::Sky &sky, const PostprocessDescriptorTableStore &postprocess);
			}
		}

		namespace PerViewCmdBuffers
		{
			class DeferredCmdBuffersProvider;
		}

		namespace RenderPipeline::RenderPasses
		{
			class PipelineROPTargets;
		}

		class Sky
		{
			enum
			{
				ROOT_PARAM_DESC_TABLE,
				ROOT_PARAM_PERFRAME_DATA_CBV,
				ROOT_PARAM_CAM_SETTINGS_CBV,
				ROOT_PARAM_COUNT
			};

		private:
			Impl::TrackedResource<ID3D12Resource> cubemap;
			WRL::ComPtr<ID3D12DescriptorHeap> SRV;
			float lumScale;

		private:
			friend extern void __cdecl ::InitRenderer();
			static WRL::ComPtr<ID3D12RootSignature> rootSig, CreateRootSig();
			static WRL::ComPtr<ID3D12PipelineState> PSO, CreatePSO();

		private:
			struct ROPBindings;
			typedef std::allocator_traits<std::pmr::polymorphic_allocator<ROPBindings>> ROPBindingsAllocatorTraits;
			class ROPBindingsDeleter final
			{
				ROPBindingsAllocatorTraits::allocator_type allocator;

			public:
				explicit ROPBindingsDeleter(ROPBindingsAllocatorTraits::allocator_type allocator) noexcept : allocator(allocator) {}

			public:
				void operator ()(ROPBindingsAllocatorTraits::pointer target);
			};

		private:
			static RenderPipeline::PipelineStage RecordRenderCommands(UINT64 cameraSettingsGPUAddress, D3D12_GPU_DESCRIPTOR_HANDLE skyboxDescriptorTable,
				PerViewCmdBuffers::DeferredCmdBuffersProvider viewCmdBuffersProvider, std::unique_ptr<ROPBindingsAllocatorTraits::value_type, ROPBindingsDeleter> ROPBindings);

		public:
			explicit Sky(const Renderer::Texture &cubemap, float lumScale);

			// define outside to break dependency on TrackedResource/ComPtr`s implementation
		protected:
			Sky(const Sky &);
			Sky(Sky &&);
			Sky &operator =(const Sky &), &operator =(Sky &&);
			~Sky();

		protected:
			D3D12_CPU_DESCRIPTOR_HANDLE GetCubemapSRV() const;
			void Render(UINT64 cameraSettingsGPUAddress, D3D12_GPU_DESCRIPTOR_HANDLE skyboxDescriptorTable,
				PerViewCmdBuffers::DeferredCmdBuffersProvider viewCmdBuffersProvider, const RenderPipeline::RenderPasses::PipelineROPTargets &ROPTargets) const;

		public:
			inline float GetLumScale() const noexcept { return lumScale; }
		};
	}

	class Sky : public Impl::Sky
	{
		friend auto Impl::Descriptors::GPUDescriptorHeap::StreamPerFrameDescriptorTables(const Renderer::Sky &sky, const PostprocessDescriptorTableStore &postprocess) -> PerFrameDescriptorTables;
		friend class Impl::World;

	public:
		using Impl::Sky::Sky;

		// hide from protected
	private:
		using Impl::Sky::GetCubemapSRV;
		using Impl::Sky::Render;
	};
}