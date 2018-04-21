/**
\author		Alexey Shaydurov aka ASH
\date		21.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "CB register.h"
#include "per-frame data.h"

namespace Renderer::Impl::GlobalGPUBuffer
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) AABB_3D_VisColors
	{
		CBRegister::AlignedRow<3> culled, rendered[2]/*phase 1 - phase 2*/;

	public:
		static inline auto CB_offset(bool visible) noexcept { return World::PerFrameData::CB_size() + sizeof(AABB_3D_VisColors) * visible; }
		static constexpr auto CB_size() noexcept { return sizeof(AABB_3D_VisColors [2]); }
	};

	static constexpr auto BoxIB_offset() { return World::PerFrameData::CB_size() + AABB_3D_VisColors::CB_size(); }
}