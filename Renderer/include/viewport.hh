#pragma once

#define NOMINMAX

#include <limits>
#include <memory>
#include <wrl/client.h>
#include "../tracked resource.h"
#include "../tracked ref.h"
#include "../per-view cmd buffers.h"
#include "../world view context.h"
#include "../render pipeline.h"

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12Resource;
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
				COMPUTE_ROOT_PARAM_LOD,
				COMPUTE_ROOT_PARAM_COUNT
			};

			enum
			{
				GFX_ROOT_PARAM_DESC_TABLE,
				GFX_ROOT_PARAM_CAM_SETTINGS_CBV,
				GFX_ROOT_PARAM_COUNT
			};

		private:
			mutable PerViewCmdBuffers::CmdBuffersManager viewCmdBuffersManager;
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
				WRL::ComPtr<ID3D12RootSignature> compute, gfx;
			};
			static PostprocessRootSigs postprocessRootSigs, CreatePostprocessRootSigs();
			static WRL::ComPtr<ID3D12PipelineState> luminanceTextureReductionPSO, CreateLuminanceTextureReductionPSO();
			static WRL::ComPtr<ID3D12PipelineState> luminanceBufferReductionPSO, CreateLuminanceBufferReductionPSO();
			static WRL::ComPtr<ID3D12PipelineState> lensFlarePSO, CreateLensFlarePSO();
			static WRL::ComPtr<ID3D12PipelineState> COC_pass_PSO, Create_COC_pass_PSO();
			static WRL::ComPtr<ID3D12PipelineState> DOF_splatting_PSO, Create_DOF_splatting_PSO();
			static WRL::ComPtr<ID3D12PipelineState> bokehCompositePSO, CreateBokehCompositePSO();
			static WRL::ComPtr<ID3D12PipelineState> brightPassPSO, CreateBrightPassPSO();
			static WRL::ComPtr<ID3D12PipelineState> bloomDownsamplePSO, CreateBloomDownsmplePSO();
			static WRL::ComPtr<ID3D12PipelineState> bloomUpsampleBlurPSO, CreateBloomUpsmpleBlurPSO();
			static WRL::ComPtr<ID3D12PipelineState> postrpocessFinalCompositePSO, CreatePostprocessFinalCompositePSO();

		private:
			RenderPipeline::PipelineStage
				Pre(PerViewCmdBuffers::DeferredCmdBuffersProvider cmdListProvider, ID3D12Resource *output, const class OffscreenBuffers &offscreenBuffers) const,
				Post(PerViewCmdBuffers::DeferredCmdBuffersProvider cmdListProvider, ID3D12Resource *output, const class OffscreenBuffers &offscreenBuffers,
					D3D12_GPU_DESCRIPTOR_HANDLE postprocessDescriptorTable, UINT width, UINT height) const;

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
			void Render(ID3D12Resource *output, const class OffscreenBuffers &offscreenBuffers, UINT width, UINT height) const;
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