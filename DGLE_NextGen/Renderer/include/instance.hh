/**
\author		Alexey Shaydurov aka ASH
\date		22.08.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <memory>
#include "object 3D.hh"
#include "../AABB.h"
#if defined _MSC_VER && (_MSC_VER == 1914 || _MSC_VER == 1915)
#include "world.hh"	// for Allocator
#endif

struct ID3D12GraphicsCommandList2;

namespace Renderer
{
	class World;

	namespace Impl
	{
		class World;

		class Instance
		{
			std::shared_ptr<class Renderer::World> world;
			Renderer::Object3D object;
			float worldXform[4][3];
			AABB<3> worldAABB;

		protected:
			/*
				to be set by World
				D3D12_GPU_VIRTUAL_ADDRESS is a synonym of UINT64 according to MSDN, use UINT64 here to avoid dependency on d3d12.h
				it is also possible to store index instead of address, it potentially can save storage (4 bytess instead of 8) but probably will not due to alignment
			*/
			UINT64 CB_GPU_ptr;

		protected:
			Instance(std::shared_ptr<class Renderer::World> &&world, Renderer::Object3D &&object, const float (&xform)[4][3], const AABB<3> &worldAABB);
			~Instance();
			Instance(Instance &) = delete;
			void operator =(Instance &) = delete;

		public:
			const auto &GetWorld() const noexcept { return world; }
			const auto &GetObject3D() const noexcept { return object; }
			const auto &GetWorldXform() const noexcept { return worldXform; }
			const auto &GetWorldAABB() const noexcept { return worldAABB; }

		protected:
			static const auto &GetRootSignature() noexcept { return Object3D::GetRootSignature(); }
			const auto &GetStartPSO() const { return object.GetStartPSO(); }
			void Render(ID3D12GraphicsCommandList2 *target) const;

#if defined _MSC_VER && (_MSC_VER == 1914 || _MSC_VER == 1915)
			template<typename>
			friend class Impl::World::Allocator;
#endif
		};
	}

	class Instance final : public Impl::Instance
	{
		friend class Impl::World;

	private:
		using Impl::Instance::Instance;
	};
}