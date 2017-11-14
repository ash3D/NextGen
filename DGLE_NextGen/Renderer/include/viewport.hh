/**
\author		Alexey Shaydurov aka ASH
\date		15.11.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <limits>
#include <memory>
#include "../frame versioning.h"

struct ID3D12GraphicsCommandList1;
struct ID3D12Resource;
struct D3D12_CPU_DESCRIPTOR_HANDLE;

namespace Renderer
{
	class World;

	namespace Impl
	{
		class World;

		using std::numeric_limits;
		using std::shared_ptr;

		static_assert(numeric_limits<double>::has_infinity);

		class Viewport
		{
			template<class Cmd>
			struct PrePostCmds
			{
				Cmd pre, post;
			};
			mutable class CmdListsManager : FrameVersioning<PrePostCmds<CmdBuffer>>
			{
			public:
				PrePostCmds<ID3D12GraphicsCommandList1 *> OnFrameStart();
				using FrameVersioning::OnFrameFinish;
			} cmdListsManager;

		private:
			const shared_ptr<const class Renderer::World> world;
			float viewXform[4][3], projXform[4][4];

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
			void Render(ID3D12Resource *rt, const D3D12_CPU_DESCRIPTOR_HANDLE &rtv, UINT width, UINT height) const;
		};
	}

	class Viewport final : public Impl::Viewport
	{
		friend class Impl::World;
		friend class RenderOutput;

		using Impl::Viewport::Viewport;
	};
}