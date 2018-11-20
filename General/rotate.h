#pragma once

#include "CompilerCheck.h"
#include <type_traits>
#include <limits>
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
			typedef uint_fast##b##_t fast, type;							\
		};

	SELECT_TYPE_RANGE(0, 8)
	SELECT_TYPE_RANGE(8, 16)
	SELECT_TYPE_RANGE(16, 32)
	SELECT_TYPE_RANGE(32, 64)

#	undef SELECT_TYPE_RANGE

#	define SELECT_TYPE_EXACT(x)					\
		template<>								\
		struct SelectType<x>					\
		{										\
			typedef uint##x##_t exact, type;	\
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

	template<unsigned width>
#if USE_BOOST_TYPE_SELECTOR
	inline auto rol(typename boost::uint_t<width>::fast value, unsigned int shift) noexcept ->
		enable_if_t<!Has_exact<typename boost::uint_t<width>>::value, decltype(value)>
#else
	inline auto rol(typename SelectType<width>::fast value, unsigned int shift) noexcept -> decltype(value)
#endif
	{
		shift %= width;
		constexpr auto mask = Mask<width, decltype(value)>;
		return value & ~mask | (value << shift | (value & mask) >> width - shift) & mask;
	}

	template<unsigned width>
#if USE_BOOST_TYPE_SELECTOR
#if defined _MSC_VER && _MSC_VER <= 1914 && !defined __clang__
	inline typename boost::uint_t<width>::exact rol(typename boost::uint_t<width>::exact value, unsigned int shift) noexcept
#else
	inline auto rol(typename boost::uint_t<width>::exact value, unsigned int shift) noexcept -> decltype(value)
#endif
#else
#if defined _MSC_VER && _MSC_VER <= 1914 && !defined __clang__
	inline typename SelectType<width>::exact rol(typename SelectType<width>::exact value, unsigned int shift) noexcept
#else
	inline auto rol(typename SelectType<width>::exact value, unsigned int shift) noexcept -> decltype(value)
#endif
#endif
	{
		shift %= width;
		return value << shift | value >> width - shift;
	}

	template<unsigned width>
#if USE_BOOST_TYPE_SELECTOR
	inline auto ror(typename boost::uint_t<width>::fast value, unsigned int shift) noexcept ->
		enable_if_t<!Has_exact<typename boost::uint_t<width>>::value, decltype(value)>
#else
	inline auto ror(typename SelectType<width>::fast value, unsigned int shift) noexcept -> decltype(value)
#endif
	{
		shift %= width;
		constexpr auto mask = Mask<width, decltype(value)>;
		return value & ~mask | ((value & mask) >> shift | value << width - shift) & mask;
	}

	template<unsigned width>
#if USE_BOOST_TYPE_SELECTOR
#if defined _MSC_VER && _MSC_VER <= 1914 && !defined __clang__
	inline typename boost::uint_t<width>::exact ror(typename boost::uint_t<width>::exact value, unsigned int shift) noexcept
#else
	inline auto ror(typename boost::uint_t<width>::exact value, unsigned int shift) noexcept -> decltype(value)
#endif
#else
#if defined _MSC_VER && _MSC_VER <= 1914 && !defined __clang__
	inline typename SelectType<width>::exact ror(typename SelectType<width>::exact value, unsigned int shift) noexcept
#else
	inline auto ror(typename SelectType<width>::exact value, unsigned int shift) noexcept -> decltype(value)
#endif
#endif
	{
		shift %= width;
		return value >> shift | value << width - shift;
	}

#ifdef _MSC_VER
#if USE_BOOST_TYPE_SELECTOR
	template<unsigned width>
	using SelectIntrinType = typename boost::uint_t<width>::exact;
#else
	template<unsigned width>
	using SelectIntrinType = typename SelectType<width>::type;
#endif

#define ROT_SPECIALIZATIONS(dir)																		\
	template<>																							\
	inline auto ro##dir<8>(SelectIntrinType<8> value, unsigned int shift) noexcept -> decltype(value)	\
	{																									\
		return _rot##dir##8(value, shift);																\
	}																									\
																										\
	template<>																							\
	inline auto ro##dir<16>(SelectIntrinType<16> value, unsigned int shift) noexcept -> decltype(value)	\
	{																									\
		return _rot##dir##16(value, shift);																\
	}																									\
																										\
	template<>																							\
	inline auto ro##dir<32>(SelectIntrinType<32> value, unsigned int shift) noexcept -> decltype(value)	\
	{																									\
		return _rot##dir(value, shift);																	\
	}																									\
																										\
	template<>																							\
	inline auto ro##dir<64>(SelectIntrinType<64> value, unsigned int shift) noexcept -> decltype(value)	\
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

	template<typename Value>
	static constexpr auto Width = numeric_limits<make_unsigned_t<Value>>::digits;
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