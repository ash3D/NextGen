#pragma once

#include "CompilerCheck.h"
#include <cassert>
#include <type_traits>
#include <bit>

template<unsigned alignment, typename UInt>
constexpr inline auto AlignSize(UInt x)
{
	static_assert(std::is_unsigned_v<UInt>, "alignment can only be applied to unsigned integral");
	static_assert(std::has_single_bit(alignment), "alignment should be power of 2");
	return x + alignment - 1 & ~(alignment - 1);
}

template<typename UInt>
inline auto AlignSize(UInt x, unsigned alignment)
{
	static_assert(std::is_unsigned_v<UInt>, "alignment can only be applied to unsigned integral");
	assert(std::has_single_bit(alignment));
	return x + alignment - 1 & ~(alignment - 1);
}