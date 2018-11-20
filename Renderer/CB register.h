#pragma once

#include "stdafx.h"

namespace Renderer::Impl::CBRegister
{
	static constexpr unsigned int bits = D3D12_COMMONSHADER_CONSTANT_BUFFER_COMPONENTS * D3D12_COMMONSHADER_CONSTANT_BUFFER_COMPONENT_BIT_COUNT, align = bits / CHAR_BIT;
	static_assert(bits % CHAR_BIT == 0);

	template<unsigned int length>
	struct alignas(align) AlignedRow : std::array<float, length>
	{
		static_assert(length <= 4, "CB register can have at most 4 components");

		void operator =(const float (&src)[length]) volatile noexcept
		{
			memcpy(const_cast<AlignedRow *>(this)->data(), src, sizeof src);
		}
	};
}