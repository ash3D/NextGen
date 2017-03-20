/**
\author		Alexey Shaydurov aka ASH
\date		20.03.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "CompilerCheck.h"
#include <type_traits>
#include <limits>
#include <functional>
#include <cstdint>
#if USE_BOOST_TYPE_SELECTOR
#include <boost/integer.hpp>
#endif
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace RotImpl
{
	using namespace std;

	template<unsigned width, typename Value>
	static constexpr auto Mask = (Value(1) << width) - Value(1);

#if USE_BOOST_TYPE_SELECTOR
	template<typename T, typename = void>
	struct Has_exact : false_type {};

	template<typename T>
	struct Has_exact<T, void_t<typename T::exact>> : true_type {};
#else
	template<unsigned width, typename Void = void>
	struct SelectType
	{
		static_assert(is_same_v<Void, void>);
		typedef uintmax_t fast;
	};

	// (a, b]
#	define SELECT_TYPE_RANGE(a, b)											\
		template<unsigned width>											\
		struct SelectType<width, enable_if_t<(width > a && width <= b)>>	\
		{																	\
			typedef uint_fast##b##_t fast;									\
		};

	SELECT_TYPE_RANGE(0, 8)
	SELECT_TYPE_RANGE(8, 16)
	SELECT_TYPE_RANGE(16, 32)
	SELECT_TYPE_RANGE(32, 64)

#	undef SELECT_TYPE_RANGE

#	define SELECT_TYPE_EXACT(x)			\
		template<>						\
		struct SelectType<x>			\
		{								\
			typedef uint##x##_t exact;	\
		};

#	ifdef UINT8_MAX
		SELECT_TYPE_EXACT(8)
#	endif

#	ifdef UINT16_MAX
		SELECT_TYPE_EXACT(16)
#	endif

#	ifdef UINT32_MAX
		SELECT_TYPE_EXACT(32)
#	endif

#	ifdef UINT64_MAX
		SELECT_TYPE_EXACT(64)
#	endif

#	undef SELECT_TYPE_EXACT
#endif

#ifdef _MSC_VER
#	define INTRINSICS_WIDTH_LIST 8, 16, 32, 64
#else	// empty
#	define INTRINSICS_WIDTH_LIST
#endif

#if defined _MSC_VER && _MSC_VER <= 1910 && !defined __clang__
	template<bool ...notFound>
	constexpr bool IntrinsicsNotFoundImpl = conjunction_v<bool_constant<notFound>...>;

	template<unsigned width, unsigned ...intrinsicWidth>
	constexpr bool IntrinsicsNotFound = IntrinsicsNotFoundImpl<width != intrinsicWidth ...>;
#else
	template<unsigned width, unsigned ...intrinsicWidth>
	constexpr bool IntrinsicsNotFound = conjunction_v<bool_constant<width != intrinsicWidth>...>;
#endif

	template<unsigned width>
	constexpr bool IsGeneric = IntrinsicsNotFound<width, INTRINSICS_WIDTH_LIST>;

#undef INTRINSICS_WIDTH_LIST

	template<unsigned width>
#if USE_BOOST_TYPE_SELECTOR
	inline auto rol(typename boost::uint_t<width>::fast value, unsigned int shift) noexcept ->
		enable_if_t<IsGeneric<width> && !Has_exact<typename boost::uint_t<width>>::value, decltype(value)>
#else
	inline auto rol(typename SelectType<width>::fast value, unsigned int shift) noexcept ->
		enable_if_t<IsGeneric<width>, decltype(value)>
#endif
	{
		shift %= width;
		constexpr auto mask = Mask<width, decltype(value)>;
		return value & ~mask | (value << shift | (value & mask) >> width - shift) & mask;
	}

	template<unsigned width>
#if USE_BOOST_TYPE_SELECTOR
	inline auto rol(typename boost::uint_t<width>::exact value, unsigned int shift) noexcept ->
#else
	inline auto rol(typename SelectType<width>::exact value, unsigned int shift) noexcept ->
#endif
		enable_if_t<IsGeneric<width>, decltype(value)>
	{
		shift %= width;
		return value << shift | value >> width - shift;
	}

	template<unsigned width>
#if USE_BOOST_TYPE_SELECTOR
	inline auto ror(typename boost::uint_t<width>::fast value, unsigned int shift) noexcept ->
		enable_if_t<IsGeneric<width> && !Has_exact<typename boost::uint_t<width>>::value, decltype(value)>
#else
	inline auto ror(typename SelectType<width>::fast value, unsigned int shift) noexcept ->
		enable_if_t<IsGeneric<width>, decltype(value)>
#endif
	{
		shift %= width;
		constexpr auto mask = Mask<width, decltype(value)>;
		return value & ~mask | ((value & mask) >> shift | value << width - shift) & mask;
	}

	template<unsigned width>
#if USE_BOOST_TYPE_SELECTOR
	inline auto ror(typename boost::uint_t<width>::exact value, unsigned int shift) noexcept ->
#else
	inline auto ror(typename SelectType<width>::exact value, unsigned int shift) noexcept ->
#endif
		enable_if_t<IsGeneric<width>, decltype(value)>
	{
		shift %= width;
		return value >> shift | value << width - shift;
	}

#ifdef _MSC_VER
#define ROT_SPECIALIZATIONS(dir)																		\
	template<unsigned width>																			\
	inline enable_if_t<width == 8, typename function<decltype(_rot##dir##8)>::result_type> ro##dir(		\
		typename function<decltype(_rot##dir##8)>::first_argument_type value,							\
		typename function<decltype(_rot##dir##8)>::second_argument_type shift)							\
	{																									\
		return _rot##dir##8(value, shift);																\
	}																									\
																										\
	template<unsigned width>																			\
	inline enable_if_t<width == 16, typename function<decltype(_rot##dir##16)>::result_type> ro##dir(	\
		typename function<decltype(_rot##dir##16)>::first_argument_type value,							\
		typename function<decltype(_rot##dir##16)>::second_argument_type shift)							\
	{																									\
		return _rot##dir##16(value, shift);																\
	}																									\
																										\
	template<unsigned width>																			\
	inline enable_if_t<width == 32, typename function<decltype(_rot##dir)>::result_type> ro##dir(		\
		typename function<decltype(_rot##dir)>::first_argument_type value,								\
		typename function<decltype(_rot##dir)>::second_argument_type shift)								\
	{																									\
		return _rot##dir(value, shift);																	\
	}																									\
																										\
	template<unsigned width>																			\
	inline enable_if_t<width == 64, typename function<decltype(_rot##dir##64)>::result_type> ro##dir(	\
		typename function<decltype(_rot##dir##64)>::first_argument_type value,							\
		typename function<decltype(_rot##dir##64)>::second_argument_type shift)							\
	{																									\
		return _rot##dir##64(value, shift);																\
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

	template<Dir dir, unsigned width, typename Value, typename Shift>
	inline auto rot_dispatch(Value value, Shift shift) -> enable_if_t<dir == Dir::Left, make_unsigned_t<common_type_t<Value, decltype(rol<width>(value, shift))>>>
	{
		return rol<width>(value, shift);
	}

	template<Dir dir, unsigned width, typename Value, typename Shift>
	inline auto rot_dispatch(Value value, Shift shift) -> enable_if_t<dir == Dir::Right, make_unsigned_t<common_type_t<Value, decltype(ror<width>(value, shift))>>>
	{
		return ror<width>(value, shift);
	}

	template<Dir dir, unsigned width, typename Value, typename Shift>
	inline auto rot(Value value, Shift shift)
	{
		static_assert(is_integral_v<Value> && is_integral_v<Shift>, "rotate is feasible for integral types only");
		static_assert(width > 0, "rotate width cannot be 0");
		static_assert(width <= numeric_limits<uintmax_t>::digits, "too large rotate width");
		return rot_dispatch<dir, width>(value, shift);
	}

	// auto does not work with VS 2015 Update 2/3 in cases such as 'const auto i = rol(1, 2);'
	template<typename Value>
	static constexpr /*auto*/size_t Width = numeric_limits<make_unsigned_t<Value>>::digits;
}

template<unsigned width, typename Value, typename Shift>
inline auto rol(Value value, Shift shift)
{
	return RotImpl::rot<RotImpl::Dir::Left, width>(value, shift);
}

template<unsigned width, typename Value, typename Shift>
inline auto ror(Value value, Shift shift)
{
	return RotImpl::rot<RotImpl::Dir::Right, width>(value, shift);
}

template<typename Value, typename Shift>
inline auto rol(Value value, Shift shift)
{
	return rol<RotImpl::Width<Value>>(value, shift);
}

template<typename Value, typename Shift>
inline auto ror(Value value, Shift shift)
{
	return ror<RotImpl::Width<Value>>(value, shift);
}