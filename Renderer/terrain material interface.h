#pragma once

#include <wrl/client.h>

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12GraphicsCommandList2;

namespace Renderer::Impl
{
	class TerrainVectorLayer;
}

namespace Renderer::TerrainMaterials
{
	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		class Interface
		{
			const WRL::ComPtr<ID3D12RootSignature> rootSig;

		protected:
			const WRL::ComPtr<ID3D12PipelineState> PSO;
			const UINT color;	// for PIX

		protected:
			enum
			{
				ROOT_PARAM_PER_FRAME_DATA_CBV,
				ROOT_PARAM_TONEMAP_PARAMS_CBV,
				GLOBAL_ROOT_PARAM_COUNT
			};

		protected:
			Interface(const WRL::ComPtr<ID3D12RootSignature> &rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO, UINT color);
			~Interface();

		public:
			Interface(Interface &) = delete;
			void operator =(Interface &) = delete;

		protected:
			void Setup(ID3D12GraphicsCommandList2 *target, UINT64 globalGPUBufferPtr, UINT64 tonemapParamsBufferPtr) const;

		private:
			virtual void FinishSetup(ID3D12GraphicsCommandList2 *target) const = 0;
		};
	}

	class Interface : protected Impl::Interface
	{
		// exposes 'PSO', 'color' and 'Setup'
		friend class Renderer::Impl::TerrainVectorLayer;

	protected:
		using Impl::Interface::Interface;
		~Interface() = default;

		// hide exposed stuff from derived classes (could alternatively define PSO & color here as private rather than inherit it)
	private:
		using Impl::Interface::PSO;
		using Impl::Interface::color;
		using Impl::Interface::Setup;

#if defined _MSC_VER && _MSC_VER <= 1920
	protected:
		struct tag {};
#endif
	};
}