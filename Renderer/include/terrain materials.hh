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
struct ID3D12GraphicsCommandList4;
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

		template<class Base>
		class TexStuff : protected DescriptorTable, public Base
		{
			class BaseHasFinishSetup;

		protected:
			const float texScale;

		protected:
			enum
			{
				ROOT_PARAM_TEXTURE_DESC_TABLE = Base::ROOT_PARAM_COUNT,
				ROOT_PARAM_SAMPLER_DESC_TABLE,
				ROOT_PARAM_QUAD_TEXGEN_REDUCTION,
				ROOT_PARAM_TEXTURE_SCALE,
				ROOT_PARAM_COUNT
			};

		protected:
			template<typename ExtraArg>
			TexStuff(float texScale, unsigned textureCount, const char materialName[], const ExtraArg &extraArg, const WRL::ComPtr<ID3D12RootSignature> &rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO);
			~TexStuff() = default;

		private:
			void SetupQuad(ID3D12GraphicsCommandList4 *target, HLSL::float2 quadCenter) const override;

		protected:
			// Inherited via Interface
			virtual void FinishSetup(ID3D12GraphicsCommandList4 *target) const override;
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
			ROOT_PARAM_ALBEDO = ROOT_PARAM_COUNT,
			ROOT_PARAM_COUNT
		};
		static void FillRootParams(CD3DX12_ROOT_PARAMETER1 dst[]);

	protected:
		explicit Flat(const float (&albedo)[3], const WRL::ComPtr<ID3D12RootSignature> &rootSig = rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO = PSO);
#if defined _MSC_VER && _MSC_VER <= 1921
	public:
		explicit Flat(tag, const float (&albedo)[3], const WRL::ComPtr<ID3D12RootSignature> &rootSig = rootSig, const WRL::ComPtr<ID3D12PipelineState> &PSO = PSO);
#else
	protected:
		~Flat() = default;
#endif

	public:
		static std::shared_ptr<Interface> __cdecl Make(const float (&albedo)[3]);

	protected:
		// Inherited via Interface
		virtual void FinishSetup(ID3D12GraphicsCommandList4 *target) const override;
	};

	class Textured final : public Impl::TexStuff<Flat>
	{
		template<class>
		friend class Misc::AllocatorProxyAdaptor;

	private:
		friend extern void __cdecl ::InitRenderer();
		static WRL::ComPtr<ID3D12RootSignature> rootSig, CreateRootSig();
		static WRL::ComPtr<ID3D12PipelineState> PSO, CreatePSO();

	private:
		const Renderer::Impl::TrackedResource<ID3D12Resource> tex;

#if defined _MSC_VER && _MSC_VER <= 1921
	public:
		explicit Textured(tag, const float (&albedoFactor)[3], const Texture &tex, float texScale, const char materialName[]);
		~Textured();
#else
	private:
		explicit Textured(const float (&albedoFactor)[3], const Texture &tex, float texScale, const char materialName[]);
		~Textured();
#endif

	public:
		static std::shared_ptr<Interface> __cdecl Make(const float (&albedo)[3], const Texture &tex, float texScale, const char materialName[]);
	};

	class Standard final : public Impl::TexStuff<Interface>
	{
		template<class>
		friend class Misc::AllocatorProxyAdaptor;

	private:
		friend extern void __cdecl ::InitRenderer();
		static WRL::ComPtr<ID3D12RootSignature> rootSig, CreateRootSig();
		static WRL::ComPtr<ID3D12PipelineState> PSO, CreatePSO();

	private:
		enum
		{
			ALBEDO_MAP,
			ROUGHNESS_MAP,
			NORMAL_MAP,
			TEXTURE_COUNT
		};
		const Renderer::Impl::TrackedResource<ID3D12Resource> textures[TEXTURE_COUNT];
		const float F0;

	private:
		enum
		{
			ROOT_PARAM_F0 = ROOT_PARAM_COUNT,
			ROOT_PARAM_COUNT
		};

#if defined _MSC_VER && _MSC_VER <= 1921
	public:
		explicit Standard(tag, const Texture &albedoMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, float IOR, const char materialName[]);
		~Standard();
#else
	private:
		explicit Standard(const Texture &albedoMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, float IOR, const char materialName[]);
		~Standard();
#endif

	public:
		static std::shared_ptr<Interface> __cdecl Make(const Texture &albedoMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, float IOR, const char materialName[]);

	private:
		// Inherited via Interface
		virtual void FinishSetup(ID3D12GraphicsCommandList4 *target) const override;
	};

	class Extended final : public Impl::TexStuff<Interface>
	{
		template<class>
		friend class Misc::AllocatorProxyAdaptor;

	private:
		friend extern void __cdecl ::InitRenderer();
		static WRL::ComPtr<ID3D12RootSignature> rootSig, CreateRootSig();
		static WRL::ComPtr<ID3D12PipelineState> PSO, CreatePSO();

	private:
		enum
		{
			ALBEDO_MAP,
			FRESNEL_MAP,
			ROUGHNESS_MAP,
			NORMAL_MAP,
			TEXTURE_COUNT
		};
		const Renderer::Impl::TrackedResource<ID3D12Resource> textures[TEXTURE_COUNT];

#if defined _MSC_VER && _MSC_VER <= 1921
	public:
		explicit Extended(tag, const Texture &albedoMap, const Texture &fresnelMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, const char materialName[]);
		~Extended();
#else
	private:
		explicit Extended(const Texture &albedoMap, const Texture &fresnelMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, const char materialName[]);
		~Extended();
#endif

	public:
		static std::shared_ptr<Interface> __cdecl Make(const Texture &albedoMap, const Texture &fresnelMap, const Texture &roughnessMap, const Texture &normalMap, float texScale, const char materialName[]);
	};
}