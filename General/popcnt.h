#pragma once

#include "CompilerCheck.h"
#include <cstdint>
#include <limits>
#include <type_traits>
#ifdef _MSC_VER
#include <intrin.h>
#endif

// call as popcnt<>(x) to bypass non-constexpr intrinsics
template<typename Int>
constexpr inline unsigned popcnt(Int x)
{
	using namespace std;
	static_assert(is_integral_v<Int>, "x should be integral");
	typedef make_unsigned_t<Int> UInt;
	UInt y = x;
	unsigned count = 0;
	for (unsigned i = 0; i < numeric_limits<UInt>::digits; i++, y >>= 1)
		count += y & 1u;
	return count;
}

// intrinsics accelerators (not constexpr!)\
! need to check for CPU support before using
#ifdef _MSC_VER
inline auto popcnt(char x)
{
	return __popcnt16(x);
}

inline auto popcnt(unsigned char x)
{
	return __popcnt16(x);
}

inline auto popcnt(signed char x)
{
	return __popcnt16(x);
}

inline auto popcnt(unsigned short int x)
{
	return __popcnt16(x);
}

inline auto popcnt(signed short int x)
{
	return __popcnt16(x);
}

inline auto popcnt(unsigned int x)
{
	return __popcnt(x);
}

inline auto popcnt(signed int x)
{
	return __popcnt(x);
}

inline auto popcnt(unsigned long int x)
{
	return __popcnt(x);
}

inline auto popcnt(signed long int x)
{
	return __popcnt(x);
}

#ifdef _M_X64
inline auto popcnt(unsigned long long int x)
{
	return __popcnt64(x);
}

inline auto popcnt(signed long long int x)
{
	return __popcnt64(x);
}
#else
inline auto popcnt(unsigned long long int x)
{
	typedef unsigned long int Half;
	return popcnt(Half(x)) + popcnt(Half(x >> CHAR_BIT * sizeof(Half)));
}

inline auto popcnt(signed long long int x)
{
	return popcnt((unsigned long long int)x);
}
#endif
#endif