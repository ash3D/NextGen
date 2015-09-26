/**
\author		Alexey Shaydurov aka ASH
\date		26.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <type_traits>
#include <limits>
#include <cstdint>
#include <boost/integer.hpp>
#include <intrin.h>

namespace RotImpl
{
	template<unsigned n>
	struct RotL
	{
		static auto apply(typename boost::uint_t<n>::fast value, unsigned int shift) noexcept -> decltype(value);
	};

	template<unsigned n>
	auto RotL<n>::apply(typename boost::uint_t<n>::fast value, unsigned int shift) noexcept -> decltype(value)
	{
		shift %= n;
		const auto mask = (decltype(value)(1) << n) - decltype(value)(1);
		return value & ~mask | (value << shift | (value & mask) >> n - shift) & mask;
	}

	template<>
	struct RotL<8>
	{
		template<typename Value, typename Shift>
		static auto apply(Value value, Shift shift) -> decltype(_rotl8(value, shift))
		{
			return _rotl8(value, shift);
		}
	};

	template<>
	struct RotL<16>
	{
		template<typename Value, typename Shift>
		static auto apply(Value value, Shift shift) -> decltype(_rotl16(value, shift))
		{
			return _rotl16(value, shift);
		}
	};

	template<>
	struct RotL<32>
	{
		template<typename Value, typename Shift>
		static auto apply(Value value, Shift shift) -> decltype(_rotl(value, shift))
		{
			return _rotl(value, shift);
		}
	};

	template<>
	struct RotL<64>
	{
		template<typename Value, typename Shift>
		static auto apply(Value value, Shift shift) -> decltype(_rotl64(value, shift))
		{
			return _rotl64(value, shift);
		}
	};
}

template<unsigned n, typename Value, typename Shift>
inline auto rotl(Value value, Shift shift) -> typename std::make_unsigned<typename std::common_type<Value, decltype(RotImpl::RotL<n>::apply(value, shift))>::type>::type
{
	static_assert(std::is_integral<Value>::value && std::is_integral<Shift>::value, "rotate works for integral types only");
	static_assert(n <= std::numeric_limits<std::uintmax_t>::digits, "too large n");
	return RotImpl::RotL<n>::apply(value, shift);
}

template<typename Value, typename Shift>
inline auto rotl(Value value, Shift shift) -> decltype(rotl<std::numeric_limits<std::make_unsigned<Value>::type>::digits>(value, shift))
{
	return rotl<std::numeric_limits<std::make_unsigned<Value>::type>::digits>(value, shift);
}