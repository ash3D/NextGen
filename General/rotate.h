/**
\author		Alexey Shaydurov aka ASH
\date		26.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <type_traits>
#include <intrin.h>

template<unsigned n, typename T>
struct TRotImpl
{
	static T apply(T value, int shift) noexcept;
};

template<unsigned n, typename T>
T TRotImpl<n, T>::apply(T value, int shift)
{
	shift %= n;
	const auto mask = (1 << n) - 1;
	return value & ~mask | (value << shift | (value & mask) >> n - shift) & mask;
}

template<typename T>
struct TRotImpl<8, T>
{
	static auto apply(T value, int shift) -> decltype(_rotl8(value, shift))
	{
		return _rotl8(value, shift);
	}
};

template<typename T>
struct TRotImpl<16, T>
{
	static auto apply(T value, int shift) -> decltype(_rotl16(value, shift))
	{
		return _rotl16(value, shift);
	}
};

template<typename T>
struct TRotImpl<32, T>
{
	static auto apply(T value, int shift) -> decltype(_rotl(value, shift))
	{
		return _rotl(value, shift);
	}
};

template<typename T>
struct TRotImpl<64, T>
{
	static auto apply(T value, int shift) -> decltype(_rotl64(value, shift))
	{
		return _rotl64(value, shift);
	}
};

template<unsigned n, typename T>
inline auto rotl(T value, int shift) -> decltype(TRotImpl<n, T>::apply(value, shift))
{
	static_assert(std::is_integral<T>::value, "rotate works for integral types only");
	static_assert(n <= sizeof(T) * 8, "too large n");
	return TRotImpl<n, T>::apply(value, shift);
}