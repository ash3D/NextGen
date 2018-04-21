/**
\author		Alexey Shaydurov aka ASH
\date		21.04.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "CompilerCheck.h"
#include "popcnt.h"
#include <cassert>
#include <type_traits>

template<unsigned alignment, typename UInt>
constexpr inline auto AlignSize(UInt x)
{
	static_assert(std::is_unsigned_v<UInt>, "alignment should be unsigned integral");
	static_assert(popcnt<>(alignment) == 1, "alignemt should be power of 2");
	return x + alignment - 1 & ~(alignment - 1);
}

template<typename UInt>
inline auto AlignSize(UInt x, unsigned alignment)
{
	static_assert(std::is_unsigned_v<UInt>, "alignment should be unsigned integral");
	assert(popcnt(alignment) == 1);
	return x + alignment - 1 & ~(alignment - 1);
}