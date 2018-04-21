/**
\author		Alexey Shaydurov aka ASH
\date		21.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "world.hh"
#include "frame versioning.h"
#include "CB register.h"

struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) Renderer::Impl::World::PerFrameData
{
	CBRegister::AlignedRow<4> projXform[4];
	CBRegister::AlignedRow<3> viewXform[4], terrainXform[4];

public:
	static inline auto CurFrameCB_offset() noexcept { return sizeof(PerFrameData) * globalFrameVersioning->GetContinuousRingIdx(); }
	static constexpr auto CB_footprint() noexcept { return sizeof(PerFrameData) * maxFrameLatency; }
};