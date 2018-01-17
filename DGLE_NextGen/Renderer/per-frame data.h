/**
\author		Alexey Shaydurov aka ASH
\date		17.01.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "frame versioning.h"

namespace Renderer::Impl
{
	static constexpr unsigned int CBRegisterBits = D3D12_COMMONSHADER_CONSTANT_BUFFER_COMPONENTS * D3D12_COMMONSHADER_CONSTANT_BUFFER_COMPONENT_BIT_COUNT, CBRegisterAlign = CBRegisterBits / CHAR_BIT;
	static_assert(CBRegisterBits % CHAR_BIT == 0);

	template<unsigned int length>
	struct alignas(CBRegisterAlign) CBRegisterAlignedRow : std::array<float, length>
	{
		void operator =(const float (&src)[length]) volatile noexcept
		{
			memcpy(const_cast<CBRegisterAlignedRow *>(this)->data(), src, sizeof src);
		}
	};

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) World::PerFrameData
	{
		CBRegisterAlignedRow<4> projXform[4];
		CBRegisterAlignedRow<3> viewXform[4], worldXform[4];

	public:
		static inline auto CurFrameCB_offset() noexcept { return sizeof(PerFrameData) * globalFrameVersioning->GetContinuousRingIdx(); }
	};
}