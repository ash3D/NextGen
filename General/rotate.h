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
	using namespace std;

	// TODO: use C++14 variable template when VS will support it
#if 1
	template<unsigned n, typename Value>
	using Mask = integral_constant<decltype((Value(1) << n) - Value(1)), (Value(1) << n) - Value(1)>;
#else
	template<unsigned n, typename Value>
	constexpr static auto Mask = (Value(1) << n) - Value(1);
#endif

	template<unsigned n>
	inline auto rol(typename boost::uint_t<n>::fast value, unsigned int shift) noexcept ->
#ifdef _MSC_VER
		enable_if_t<n != 8 && n != 16 && n != 32 && n != 64, decltype(value)>
#else
		decltype(value)
#endif
	{
		shift %= n;
		constexpr auto mask = Mask<n, decltype(value)>::value;
		return value & ~mask | (value << shift | (value & mask) >> n - shift) & mask;
	}

	template<unsigned n>
	inline auto ror(typename boost::uint_t<n>::fast value, unsigned int shift) noexcept ->
#ifdef _MSC_VER
		enable_if_t<n != 8 && n != 16 && n != 32 && n != 64, decltype(value)>
#else
		decltype(value)
#endif
	{
		shift %= n;
		constexpr auto mask = Mask<n, decltype(value)>::value;
		return value & ~mask | ((value & mask) >> shift | value << n - shift) & mask;
	}

#ifdef _MSC_VER
#define ROT_SPECIALIZATIONS(dir)																	\
	template<unsigned n>																			\
	inline enable_if_t<n == 8, typename function<decltype(_rot##dir##8)>::result_type> ro##dir(		\
		typename function<decltype(_rot##dir##8)>::first_argument_type value,						\
		typename function<decltype(_rot##dir##8)>::second_argument_type shift)						\
	{																								\
		return _rot##dir##8(value, shift);															\
	}																								\
																									\
	template<unsigned n>																			\
	inline enable_if_t<n == 16, typename function<decltype(_rot##dir##16)>::result_type> ro##dir(	\
		typename function<decltype(_rot##dir##16)>::first_argument_type value,						\
		typename function<decltype(_rot##dir##16)>::second_argument_type shift)						\
	{																								\
		return _rot##dir##16(value, shift);															\
	}																								\
																									\
	template<unsigned n>																			\
	inline enable_if_t<n == 32, typename function<decltype(_rot##dir)>::result_type> ro##dir(		\
		typename function<decltype(_rot##dir)>::first_argument_type value,							\
		typename function<decltype(_rot##dir)>::second_argument_type shift)							\
	{																								\
		return _rot##dir(value, shift);																\
	}																								\
																									\
	template<unsigned n>																			\
	inline enable_if_t<n == 64, typename function<decltype(_rot##dir##64)>::result_type> ro##dir(	\
		typename function<decltype(_rot##dir##64)>::first_argument_type value,						\
		typename function<decltype(_rot##dir##64)>::second_argument_type shift)						\
	{																								\
		return _rot##dir##64(value, shift);															\
	}

	ROT_SPECIALIZATIONS(l)
	ROT_SPECIALIZATIONS(r)

#undef ROT_SPECIALIZATIONS
#endif

	enum class Dir
	{
		Left,
		Right,
	};

	template<Dir dir, unsigned n, typename Value, typename Shift>
	inline auto rot_dispatch(Value value, Shift shift) -> enable_if_t<dir == Dir::Left, make_unsigned_t<common_type_t<Value, decltype(rol<n>(value, shift))>>>
	{
		return rol<n>(value, shift);
	}

	template<Dir dir, unsigned n, typename Value, typename Shift>
	inline auto rot_dispatch(Value value, Shift shift) -> enable_if_t<dir == Dir::Right, make_unsigned_t<common_type_t<Value, decltype(rol<n>(value, shift))>>>
	{
		return ror<n>(value, shift);
	}

	template<Dir dir, unsigned n, typename Value, typename Shift>
	inline auto rot(Value value, Shift shift)
	{
		static_assert(is_integral<Value>::value && is_integral<Shift>::value, "rotate is feasible for integral types only");
		static_assert(n <= numeric_limits<uintmax_t>::digits, "too large n");
		return rot_dispatch<dir, n>(value, shift);
	}

	// TODO: use C++14 variable template when VS will support it
#if 1
	template<typename Value>
	using N = integral_constant<size_t, numeric_limits<make_unsigned_t<Value>>::digits>;
#else
	template<typename Value>
	constexpr static auto N = numeric_limits<make_unsigned_t<Value>>::digits;
#endif
}

template<unsigned n, typename Value, typename Shift>
inline auto rol(Value value, Shift shift)
{
	return RotImpl::rot<RotImpl::Dir::Left, n>(value, shift);
}

template<unsigned n, typename Value, typename Shift>
inline auto ror(Value value, Shift shift)
{
	return RotImpl::rot<RotImpl::Dir::Right, n>(value, shift);
}

template<typename Value, typename Shift>
inline auto rol(Value value, Shift shift)
{
	return rol<RotImpl::N<Value>value>(value, shift);
}

template<typename Value, typename Shift>
inline auto ror(Value value, Shift shift)
{
	return ror<RotImpl::N<Value>value>(value, shift);
}