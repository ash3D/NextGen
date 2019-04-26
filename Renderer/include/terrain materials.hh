#pragma once

#include <memory>
#include <wrl/client.h>
#include "../terrain material interface.h"
#include "../GPU descriptor heap.h"
#include "../tracked resource.h"
#include "allocator adaptors.h"

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList2;
struct ID3D12Resource;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct CD3DX12_ROOT_PARAMETER1;

extern void __cdecl InitRenderer();

namespace Renderer
{
	class Texture;
}

namespace Renderer::TerrainMaterials
{
	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		namespace Descriptors = Renderer::Impl::Descriptors;

		class DescriptorTable : protected Descriptors::GPUDescriptorHeap::AllocationClient
		{
			WRL::ComPtr<ID3D12DescriptorHeap> stage;

		protected:
			DescriptorTable(unsigned size, const char materialName[]);
			~DescriptorTable();

		protected:
			const auto &GetCPUStage() const noexcept { return stage; }

		private:
			// Inherited via AllocationClient
			virtual void Commit(D3D12_CPU_DESCRIPTOR_HANDLE dst) const override final;
		};
	}

	class Flat : public Interface
	{
		template<class>
		friend class Misc::AllocatorProxyAdaptor;

	private:
		friend extern void __cdecl ::InitRenderer();
		static WRL::ComPtr<ID3D12RootSignature> rootSig, CreateRootSig();
		static WRL::ComPtr<ID3D12PipelineState> PSO, CreatePSO();

	private:
		const float albedo[3];

	protected:
		enum
		{
			ROOT_PARAM_ALBEDO = GLOBAL_ROOT_PARAM_COUNT,
			FLAT_ROOT_PARAM_COUNT
		};
		static void FillRootParams(CD3DX12_ROOT_PARAMETER1 dst[]);

#if defined _MSC_VER && _MSC_VER <= 1920
	public:
		explicit Flat(tag, const float (&albedo)[3], const WRL::ComPtr<ID3D12RootSignature> &rootSig = rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO = PSO);
#else
	protected:
		explicit Flat(const float (&albedo)[3], const WRL::ComPtr<ID3D12RootSignature> &rootSig = rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO = PSO);
		~Flat() = default;
#endif

	public:
		static std::shared_ptr<Interface> __cdecl Make(const float (&albedo)[3]);

	protected:
		// Inherited via Interface
		virtual void FinishSetup(ID3D12GraphicsCommandList2 *target) const override;
	};

	class Textured final : Impl::DescriptorTable, public Flat
	{
		template<class>
		friend class Misc::AllocatorProxyAdaptor;

	private:
		friend extern void __cdecl ::InitRenderer();
		static WRL::ComPtr<ID3D12RootSignature> rootSig, CreateRootSig();
		static WRL::ComPtr<ID3D12PipelineState> PSO, CreatePSO();

	private:
		const Renderer::Impl::TrackedResource<ID3D12Resource> tex;
		const float texScale;

	private:
		enum
		{
			ROOT_PARAM_TEXTURE_DESC_TABLE = FLAT_ROOT_PARAM_COUNT,
			ROOT_PARAM_SAMPLER_DESC_TABLE,
			ROOT_PARAM_TEXTURE_SCALE,
			TEXTURED_ROOT_PARAM_COUNT
		};

#if defined _MSC_VER && _MSC_VER <= 1920
	public:
		explicit Textured(tag, const float (&albedo)[3], const Texture &tex, float texScale, const char materialName[]);
		~Textured();
#else
	private:
		explicit Textured(const float (&albedo)[3], const Texture &tex, float texScale, const char materialName[]);
		~Textured();
#endif

	public:
		static std::shared_ptr<Interface> __cdecl Make(const float (&albedo)[3], const Texture &tex, float texScale, const char materialName[]);

	private:
		// Inherited via Interface
		virtual void FinishSetup(ID3D12GraphicsCommandList2 *target) const override;
	};
}