/**
\author		Alexey Shaydurov aka ASH
\date		23.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "world.hh"
#include "frame versioning.h"
#include "CB register.h"

struct Renderer::Impl::World::GlobalGPUBufferData
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) alignas(D3D12_COMMONSHADER_CONSTANT_BUFFER_PARTIAL_UPDATE_EXTENTS_BYTE_ALIGNMENT) PerFrameData
	{
		CBRegister::AlignedRow<4> projXform[4];
		CBRegister::AlignedRow<3> viewXform[4], terrainXform[4];
		struct
		{
			CBRegister::AlignedRow<3> dir, radiance;
		} sun;

	public:
		static inline auto CurFrameCB_offset() noexcept { return offsetof(GlobalGPUBufferData, perFrameDataCB) + globalFrameVersioning->GetContinuousRingIdx() * sizeof(PerFrameData); }
	} perFrameDataCB[maxFrameLatency];

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) alignas(D3D12_COMMONSHADER_CONSTANT_BUFFER_PARTIAL_UPDATE_EXTENTS_BYTE_ALIGNMENT) AABB_3D_VisColors
	{
		CBRegister::AlignedRow<3> culled, rendered[2]/*phase 1 - phase 2*/;

	public:
		static inline auto CB_offset(bool visible) noexcept { return offsetof(GlobalGPUBufferData, aabbVisColorsCB) + visible * sizeof(AABB_3D_VisColors); }
	} aabbVisColorsCB[2]/*hidden - visible*/;

	/*
	LH-CW or RH-CCW
	!: handedness is currently hardcoded
	consider determining it dynamically based on transform determinant and select either appropriate IB or 'cull mode / front CCW' in PSOs
	*/
	static constexpr uint16_t boxIBInitData[14] = { 2, 3, 0, 1, 5, 3, 7, 2, 6, 0, 4, 5, 6, 7 };
	std::remove_const_t<decltype(boxIBInitData)> boxIB;

public:
	static constexpr auto BoxIB_offset() noexcept { return offsetof(GlobalGPUBufferData, boxIB); }
};