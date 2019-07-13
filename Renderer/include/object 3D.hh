#pragma once

#define NOMINMAX

#include <cstdint>
#include <utility>
#include <type_traits>
#include <memory>
#include <string>
#include <array>
#include <variant>
#include <functional>
#include <future>
#include <wrl/client.h>
#include "texture.hh"
#include "../tracked resource.h"
#include "../AABB.h"
#define DISABLE_MATRIX_SWIZZLES
#if !__INTELLISENSE__ 
#include "vector math.h"
#endif

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList2;

extern void __cdecl InitRenderer();

namespace Renderer
{
	namespace WRL = Microsoft::WRL;
	namespace HLSL = Math::VectorMath::HLSL;

	namespace Impl
	{
		class World;
		class Instance;

		class Object3D
		{
			friend extern void __cdecl ::InitRenderer();

		private:
			enum
			{
				NORMAL_MAP_FLAG = 1,
				GLASS_MASK_FLAG = NORMAL_MAP_FLAG << 1,
				ADVANCED_MATERIAL_COUNT = (GLASS_MASK_FLAG << 1) - 1
			};
			struct PSOs
			{
				WRL::ComPtr<ID3D12PipelineState> flat, tex[2], TV, advanced[ADVANCED_MATERIAL_COUNT];
			};

		private:
			static WRL::ComPtr<ID3D12RootSignature> rootSig, CreateRootSig();
			static std::array<PSOs, 2> PSOs, CreatePSOs();

		private:
			struct Context;
			struct Subobject;

		public:
			struct SubobjectDataBase
			{
				AABB<3> aabb;
				unsigned short int vcount, tricount;
				bool doublesided;
				const uint16_t (*tris)[3];
				const float (*verts)[3], (*normals)[3];
			};

			struct SubobjectDataUV : SubobjectDataBase
			{
				const float (*uv)[2];
			};

			enum class SubobjectType
			{
				Flat,
				Tex,
				TV,
				Advanced,
			};

			template<SubobjectType>
			struct SubobjectData;

			template<>
			struct SubobjectData<SubobjectType::Flat> : SubobjectDataBase
			{
				float albedo[3];
			};

			template<>
			struct SubobjectData<SubobjectType::Tex> : SubobjectDataUV
			{
				Renderer::Texture albedoMap;
				bool alphatest;
			};

			template<>
			struct SubobjectData<SubobjectType::TV> : SubobjectDataUV
			{
				float albedo[3], brighntess;
				Renderer::Texture screen;
			};

			template<>
			struct SubobjectData<SubobjectType::Advanced> : SubobjectDataUV
			{
				const float (*tangents)[2][3];
				Renderer::Texture albedoMap, normalMap, glassMask;
			};

		protected:
			enum
			{
				ROOT_PARAM_PER_FRAME_DATA_CBV,
				ROOT_PARAM_TONEMAP_PARAMS_CBV,
				ROOT_PARAM_INSTANCE_DATA_CBV,
				ROOT_PARAM_MATERIAL,
				ROOT_PARAM_TEXTURE_DESC_TABLE,
				ROOT_PARAM_SAMPLER_DESC_TABLE,
				ROOT_PARAM_COUNT
			};

		private:
			class DescriptorTablePack;
			// is GPU lifetime tracking is necessary for cmd list (or is it enough for cmd allocator only)?
			std::shared_future<std::pair<Impl::TrackedResource<ID3D12CommandAllocator>, Impl::TrackedResource<ID3D12GraphicsCommandList2>>> bundle;
			std::shared_ptr<Subobject []> subobjects;
			std::shared_ptr<DescriptorTablePack> descriptorTablePack;	// serves all subobjects
			Impl::TrackedResource<ID3D12Resource> VIB;	// Vertex/Index Buffer, also contain material for Intel workaround
			unsigned long int tricount;
			unsigned short int subobjCount;

		public:	// for inherited ctor
			typedef std::function<std::variant<
				SubobjectData<SubobjectType::Flat>,
				SubobjectData<SubobjectType::Tex>,
				SubobjectData<SubobjectType::TV>,
				SubobjectData<SubobjectType::Advanced>>
				__cdecl(unsigned int subobjIdx)> SubobjectDataCallback;
			Object3D(unsigned short int subobjCount, const SubobjectDataCallback &getSubobjectData, std::string name);

		protected:
			Object3D();
			Object3D(const Object3D &);
			Object3D(Object3D &&);
			Object3D &operator =(const Object3D &), &operator =(Object3D &&);
			~Object3D();

		public:
			explicit operator bool() const noexcept { return VIB; }
			AABB<3> GetXformedAABB(const HLSL::float4x3 &xform) const;
			unsigned long int GetTriCount() const noexcept { return tricount; }

		protected:
			static void Setup(ID3D12GraphicsCommandList2 *target, UINT64 frameDataGPUPtr, UINT64 tonemapParamsGPUPtr);
			ID3D12PipelineState *GetStartPSO() const;
			const void Render(ID3D12GraphicsCommandList2 *target) const;

		private:
#ifdef _MSC_VER
			static std::decay_t<decltype(bundle.get())> CreateBundle(const decltype(subobjects) &subobjects, unsigned short int subobjCount, WRL::ComPtr<ID3D12Resource> VIB,
				unsigned long int VB_size, unsigned long int UVB_size, unsigned long int TGB_size, unsigned long int IB_size, std::wstring &&objectName);
#else
			static std::decay_t<decltype(bundle.get())> CreateBundle(const decltype(subobjects) &subobjects, unsigned short int subobjCount, WRL::ComPtr<ID3D12Resource> VIB,
				unsigned long int VB_size, unsigned long int UVB_size, unsigned long int TGB_size, unsigned long int IB_size, std::string &&objectName);
#endif
		};
	}

	class Object3D : public Impl::Object3D
	{
		friend class Impl::Instance;

	public:
		using Impl::Object3D::Object3D;

		// hide from protected
	private:
		using Impl::Object3D::Setup;
		using Impl::Object3D::GetStartPSO;
		using Impl::Object3D::Render;
	};
}