#pragma once

#include <wrl/client.h>
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12GraphicsCommandList4;
struct CD3DX12_ROOT_PARAMETER1;

namespace Renderer::TerrainMaterials
{
	namespace WRL = Microsoft::WRL;
	namespace HLSL = Math::VectorMath::HLSL;

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
				ROOT_PARAM_CAMERA_SETTINGS_CBV,
				ROOT_PARAM_COUNT
			};
			static void FillRootParams(CD3DX12_ROOT_PARAMETER1 dst[]);

		protected:
			Interface(UINT color, const WRL::ComPtr<ID3D12RootSignature> &rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO);
			~Interface();

		public:
			Interface(Interface &) = delete;
			void operator =(Interface &) = delete;

		protected:
			void Setup(ID3D12GraphicsCommandList4 *target, UINT64 globalGPUBufferPtr, UINT64 cameraSettingsBufferPtr) const;
			virtual void SetupQuad(ID3D12GraphicsCommandList4 *target, HLSL::float2 quadCenter) const {/*nop*/}

		private:
			virtual void FinishSetup(ID3D12GraphicsCommandList4 *target) const = 0;
		};
	}

	class Interface : protected Impl::Interface
	{
		// exposes 'PSO', 'color' and 'Setup'
		friend class TerrainVectorQuad;

	protected:
		using Impl::Interface::Interface;
		~Interface() = default;

		// hide exposed stuff from derived classes (could alternatively define PSO & color here as private rather than inherit it)
	private:
		using Impl::Interface::PSO;
		using Impl::Interface::color;
		using Impl::Interface::Setup;
	};
}