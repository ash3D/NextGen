/**
\author		Alexey Shaydurov aka ASH
\date		18.03.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#define NOMINMAX

#include <cstdint>
#include <utility>
#include <type_traits>
#include <memory>
#include <string>
#include <array>
#include <functional>
#include <future>
#include <wrl/client.h>
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
struct ID3D12GraphicsCommandList1;

extern void __cdecl InitRenderer();

namespace Renderer
{
	namespace WRL = Microsoft::WRL;
	namespace HLSL = Math::VectorMath::HLSL;

	namespace Impl
	{
		class Instance;

		class Object3D
		{
			friend extern void __cdecl ::InitRenderer();

		private:
			struct Subobject
			{
				HLSL::float3 color;
				AABB<3> aabb;
				unsigned long int vOffset, triOffset;
				unsigned short int tricount;
				bool doublesided;
			};

		public:
			struct SubobjectData
			{
				float color[3];
				AABB<3> aabb;
				unsigned short int vcount, tricount;
				bool doublesided;
				const float (*verts)[3];
				const uint16_t (*tris)[3];
			};

		private:
			static WRL::ComPtr<ID3D12RootSignature> rootSig, TryCreateRootSig(), CreateRootSig();
			static std::array<WRL::ComPtr<ID3D12PipelineState>, 2> PSOs, TryCreatePSOs(), CreatePSOs();

		private:
			// is GPU lifetime tracking is necessary for cmd list (or is it enough for cmd allocator only)?
			std::shared_future<std::pair<Impl::TrackedResource<ID3D12CommandAllocator>, Impl::TrackedResource<ID3D12GraphicsCommandList1>>> bundle;
			std::shared_ptr<Subobject []> subobjects;
			Impl::TrackedResource<ID3D12Resource> VIB;	// Vertex/Index Buffer, also contin material for Intel workaround
			unsigned long int tricount;
			unsigned int subobjCount;

		public:	// for inherited ctor
			Object3D(unsigned int subobjCount, const std::function<SubobjectData __cdecl(unsigned int subobjIdx)> &getSubobjectData, std::string name);

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
			static const auto &GetRootSignature() noexcept { return rootSig; }
			const WRL::ComPtr<ID3D12PipelineState> &GetStartPSO() const;
			const void Render(ID3D12GraphicsCommandList1 *target) const;

		private:
#ifdef _MSC_VER
			static std::decay_t<decltype(bundle.get())> CreateBundle(const decltype(subobjects) &subobjects, unsigned int subobjCount, WRL::ComPtr<ID3D12Resource> VIB, unsigned long int VB_size, unsigned long int IB_size, std::wstring &&objectName);
#else
			static std::decay_t<decltype(bundle.get())> CreateBundle(const decltype(subobjects) &subobjects, unsigned int subobjCount, WRL::ComPtr<ID3D12Resource> VIB, unsigned long int VB_size, unsigned long int IB_size, std::string &&objectName);
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
		using Impl::Object3D::GetRootSignature;
		using Impl::Object3D::Render;
	};
}