#pragma once

#include <memory>
#include <wrl/client.h>
#include "../terrain material interface.h"
#include "../GPU descriptor heap.h"
#include "world.hh"	// temp for Allocator

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList2;
struct D3D12_CPU_DESCRIPTOR_HANDLE;

extern void __cdecl InitRenderer();

namespace Renderer::TerrainMaterials
{
	namespace WRL = Microsoft::WRL;

	namespace Impl
	{
		class DescriptorTable : protected Renderer::Impl::Descriptors::GPUDescriptorHeap::AllocationClient
		{
			WRL::ComPtr<ID3D12DescriptorHeap> stage;

		protected:
			DescriptorTable(unsigned size, const char materialName[]);
			~DescriptorTable();

		private:
			// Inherited via AllocationClient
			virtual void Commit(D3D12_CPU_DESCRIPTOR_HANDLE dst) const override final;
		};
	}

	class Flat : public Interface
	{
		template<typename>
		friend class World::Allocator;

	private:
		friend extern void __cdecl ::InitRenderer();
		static WRL::ComPtr<ID3D12RootSignature> rootSig, TryCreateRootSig(), CreateRootSig();
		static WRL::ComPtr<ID3D12PipelineState> PSO, TryCreatePSO(), CreatePSO();

	private:
		const float albedo[3];

	protected:
		enum
		{
			ROOT_PARAM_ALBEDO = GLOBAL_ROOT_PARAM_COUNT,
			FLAT_ROOT_PARAM_COUNT
		};

#if defined _MSC_VER && _MSC_VER <= 1920
	public:
		Flat(tag, const float (&albedo)[3], const WRL::ComPtr<ID3D12RootSignature> &rootSig = rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO = PSO);
#else
	protected:
		Flat(const float (&albedo)[3], const WRL::ComPtr<ID3D12RootSignature> &rootSig = rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO = PSO);
		~Flat() = default;
#endif

	public:
		static std::shared_ptr<Interface> __cdecl Make(const float (&albedo)[3]);

	protected:
		// Inherited via Interface
		virtual void FinishSetup(ID3D12GraphicsCommandList2 *target) const override;
	};

	class Textured final : Impl::DescriptorTable, Flat
	{
		enum
		{
			ROOT_PARAM_TEXTURE_DESC_TABLE = FLAT_ROOT_PARAM_COUNT,
			TEXTURED_ROOT_PARAM_COUNT
		};

	private:
		Textured(const float (&albedo)[3]);
		~Textured() = default;

	private:
		// Inherited via Interface
		virtual void FinishSetup(ID3D12GraphicsCommandList2 *target) const override;
	};
}