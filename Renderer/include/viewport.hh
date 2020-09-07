#pragma once

#include <limits>
#include <memory>
#include "../tracked resource.h"
#include "../tracked ref.h"
#include "../frame versioning.h"
#include "../world view context.h"
#include "../render pipeline.h"
#include "../postprocess descriptor table store.h"

struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList4;
struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_GPU_DESCRIPTOR_HANDLE;

extern void __cdecl InitRenderer();

namespace Renderer
{
	class Viewport;
	class World;

	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		class World;

		using std::numeric_limits;
		using std::shared_ptr;

		static_assert(numeric_limits<double>::has_infinity);

		class Viewport : protected TrackedRef::Target<Renderer::Viewport>
		{
			enum
			{
				COMPUTE_ROOT_PARAM_DESC_TABLE,
				COMPUTE_ROOT_PARAM_CAM_SETTINGS_CBV,
				COMPUTE_ROOT_PARAM_CAM_SETTINGS_UAV,
				COMPUTE_ROOT_PARAM_PERFRAME_DATA_CBV,
				COMPUTE_ROOT_PARAM_PUSH_CONST,
				COMPUTE_ROOT_PARAM_COUNT
			};

			enum
			{
				GFX_ROOT_PARAM_DESC_TABLE,
				GFX_ROOT_PARAM_CAM_SETTINGS_CBV,
				GFX_ROOT_PARAM_COUNT
			};

		private:
			class CmdBuffsManager;

			struct PrePostCmdBuffs final
			{
				WRL::ComPtr<ID3D12CommandAllocator> allocator;
				WRL::ComPtr<ID3D12GraphicsCommandList4> pre, post;
			};

			class DeferredCmdBuffsProvider final
			{
				friend class CmdBuffsManager;

			private:
				PrePostCmdBuffs &cmdBuffers;
				const void *const viewportPtr;
				const unsigned short createVersion;

			private:
				explicit DeferredCmdBuffsProvider(PrePostCmdBuffs &cmdBuffers, const void *viewportPtr = NULL, unsigned short createVersion = ~0) :
					cmdBuffers(cmdBuffers), viewportPtr(viewportPtr), createVersion(createVersion)
				{}

			private:
				ID3D12GraphicsCommandList4 *Acquire(WRL::ComPtr<ID3D12GraphicsCommandList4> &list, const WCHAR listName[], ID3D12PipelineState *PSO);

			public:
				inline ID3D12GraphicsCommandList4 *AcquirePre(), *AcquirePost();
			};

			static constexpr const WCHAR viewportName[] = L"viewport";
			mutable class CmdBuffsManager final : FrameVersioning<PrePostCmdBuffs, viewportName>
			{
			public:
				DeferredCmdBuffsProvider OnFrameStart();
				using FrameVersioning::OnFrameFinish;
			} cmdBuffsManager;

		private:
			const shared_ptr<const class Renderer::World> world;
			mutable WorldViewContext ctx;
			float viewXform[4][3], projXform[4][4], projParams[3]/*F, 1 / zn, 1 / zn - 1 / zf*/;
			typedef std::chrono::steady_clock Clock;
			mutable Clock::time_point time = Clock::now();
			TrackedResource<ID3D12Resource> cameraSettingsBuffer;
			mutable bool fresh = true;

		private:
			friend extern void __cdecl ::InitRenderer();
			struct PostprocessRootSigs
			{
				WRL::ComPtr<ID3D12RootSignature> compute, lensFlare, DOF;
			};
			static PostprocessRootSigs postprocessRootSigs, CreatePostprocessRootSigs();
			static WRL::ComPtr<ID3D12PipelineState> luminanceTextureReductionPSO, CreateLuminanceTextureReductionPSO();
			static WRL::ComPtr<ID3D12PipelineState> luminanceBufferReductionPSO, CreateLuminanceBufferReductionPSO();
			static WRL::ComPtr<ID3D12PipelineState> lensFlarePSO, CreateLensFlarePSO();
			static WRL::ComPtr<ID3D12PipelineState> COC_pass_PSO, Create_COC_pass_PSO();
			static WRL::ComPtr<ID3D12PipelineState> DOF_splatting_PSO, Create_DOF_splatting_PSO();
			static WRL::ComPtr<ID3D12PipelineState> DOF_LF_composite_PSO, Create_DOF_LF_composite_PSO();
			static WRL::ComPtr<ID3D12PipelineState> brightPassPSO, CreateBrightPassPSO();
			static WRL::ComPtr<ID3D12PipelineState> bloomDownsamplePSO, CreateBloomDownsmplePSO();
			static WRL::ComPtr<ID3D12PipelineState> bloomUpsampleBlurPSO, CreateBloomUpsmpleBlurPSO();
			static WRL::ComPtr<ID3D12PipelineState> postrpocessFinalCompositePSO, CreatePostprocessFinalCompositePSO();

		private:
			RenderPipeline::PipelineStage
				Pre(DeferredCmdBuffsProvider cmdListProvider, ID3D12Resource *output) const,
				Post(DeferredCmdBuffsProvider cmdListProvider, ID3D12Resource *output, ID3D12Resource *ZBuffert, const TrackedResource<ID3D12Resource> (&HDRSurfaces)[2], ID3D12Resource *LDRSurface,
					ID3D12Resource *COCBuffer, ID3D12Resource *halfresDOFSurface, ID3D12Resource *DOFLayers, ID3D12Resource *lensFlareSurface,
					ID3D12Resource *bloomUpChain, ID3D12Resource *bloomDownChain, ID3D12Resource *luminanceReductionBuffer, D3D12_CPU_DESCRIPTOR_HANDLE rtvDOFLayers, D3D12_CPU_DESCRIPTOR_HANDLE rtvLensFlare,
					D3D12_GPU_DESCRIPTOR_HANDLE postprocessDescriptorTable, const Descriptors::PostprocessDescriptorTableStore &postprocessDescriptorTableStore, float camAdaptationLerpFactor, UINT width, UINT height) const;

		protected:
		public:
			explicit Viewport(shared_ptr<const Renderer::World> world);
			~Viewport() = default;
			Viewport(Viewport &) = delete;
			void operator =(Viewport &) = delete;

		public:
			void SetViewTransform(const float (&matrix)[4][3]);
			void SetProjectionTransform(double fovy, double zn, double zf = numeric_limits<double>::infinity());

		protected:
			void UpdateAspect(double invAspect);
			void Render(ID3D12Resource *output, ID3D12Resource *rendertarget, ID3D12Resource *ZBuffer, const TrackedResource<ID3D12Resource> (&HDRSurfaces)[2], ID3D12Resource *LDRSurface,
				ID3D12Resource *COCBuffer, ID3D12Resource *halfresDOFSurface, ID3D12Resource *DOFLayers, ID3D12Resource *lensFlareSurface,
				ID3D12Resource *bloomUpChain, ID3D12Resource *bloomDownChain, ID3D12Resource *luminanceReductionBuffer,
				const D3D12_CPU_DESCRIPTOR_HANDLE rtv, const D3D12_CPU_DESCRIPTOR_HANDLE dsv, const D3D12_CPU_DESCRIPTOR_HANDLE rtvDOFLayers, const D3D12_CPU_DESCRIPTOR_HANDLE rtvLensFlare,
				const D3D12_GPU_DESCRIPTOR_HANDLE postprocessDescriptorTable, const Descriptors::PostprocessDescriptorTableStore &postprocessDescriptorTableStore, UINT width, UINT height) const;
			void OnFrameFinish() const;
		};
	}

	class Viewport final : public Impl::Viewport
	{
		friend class Impl::World;
		friend class RenderOutput;
		friend class Impl::TrackedRef::Ref<Viewport>;

		using Impl::Viewport::Viewport;
	};
}