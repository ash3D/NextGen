/**
\author		Alexey Shaydurov aka ASH
\date		11.02.2016 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <type_traits>
#include <limits>
#include <functional>
#include <cstdint>
#include <boost/integer.hpp>
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace RotImpl
{
	using std::enable_if_t;

	template<unsigned n>
	inline auto rol(typename boost::uint_t<n>::fast value, unsigned int shift) noexcept -> enable_if_t<n != 8 && n != 16 && n != 32 && n != 64, decltype(value)>
	{
		shift %= n;
		const auto mask = (decltype(value)(1) << n) - decltype(value)(1);
		return value & ~mask | (value << shift | (value & mask) >> n - shift) & mask;
	}

#ifdef _MSC_VER
	using std::function;

	template<unsigned n>
	inline enable_if_t<n == 8, typename function<decltype(_rotl8)>::result_type> rol(
		typename function<decltype(_rotl8)>::first_argument_type value,
		typename function<decltype(_rotl8)>::second_argument_type shift)
	{
		return _rotl8(value, shift);
	}

	template<unsigned n>
	inline enable_if_t<n == 16, typename function<decltype(_rotl16)>::result_type> rol(
		typename function<decltype(_rotl16)>::first_argument_type value,
		typename function<decltype(_rotl16)>::second_argument_type shift)
	{
		return _rotl16(value, shift);
	}

	template<unsigned n>
	inline enable_if_t<n == 32, typename function<decltype(_rotl)>::result_type> rol(
		typename function<decltype(_rotl)>::first_argument_type value,
		typename function<decltype(_rotl)>::second_argument_type shift)
	{
		return _rotl(value, shift);
	}

	template<unsigned n>
	inline enable_if_t<n == 64, typename function<decltype(_rotl64)>::result_type> rol(
		typename function<decltype(_rotl64)>::first_argument_type value,
		typename function<decltype(_rotl64)>::second_argument_type shift)
	{
		return _rotl64(value, shift);
	}
#endif
}

template<unsigned n, typename Value, typename Shift>
inline auto rol(Value value, Shift shift) -> std::make_unsigned_t<std::common_type_t<Value, decltype(RotImpl::rol<n>(value, shift))>>
{
	static_assert(std::is_integral<Value>::value && std::is_integral<Shift>::value, "rotate works for integral types only");
	static_assert(n <= std::numeric_limits<std::uintmax_t>::digits, "too large n");
	return RotImpl::rol<n>(value, shift);
}

template<typename Value, typename Shift>
inline auto rol(Value value, Shift shift) -> decltype(rol<std::numeric_limits<std::make_unsigned_t<Value>>::digits>(value, shift))
{
	return rol<std::numeric_limits<std::make_unsigned_t<Value>>::digits>(value, shift);
}