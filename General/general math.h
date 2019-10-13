#pragma once

#include <cstdint>

namespace Math
{
	// NOTE: this implementation is designed for small values, overflows are not handled
	namespace Combinatorics
	{
		// number of k-combinations, returns 0 if k > n
		template<unsigned int n, unsigned int k = n>
		constexpr uintmax_t P = n * P<n - 1, k - 1>;

		template<>
		constexpr uintmax_t P<0, 0> = UINTMAX_C(1);

		template<unsigned int n>
		constexpr uintmax_t P<n, 0> = UINTMAX_C(1);

		template<unsigned int k>
		constexpr uintmax_t P<0, k> = UINTMAX_C(0);

		// number of k-permutations of n (binomial coefficient), returns 0 if k > n
		template<unsigned int n, unsigned int k>
		constexpr uintmax_t C = P<n, k> / P<k>;
	}

	// TODO: use C++20 auto instead of template
	template<typename A, typename B, typename T>
	inline auto lerp(const A &a, const B &b, const T &t)
	{
		return (1 - t) * a + t * b;
	}

	template<class Vec>
	class Slerp
	{
		Vec x, y;
		typename Vec::ElementType angle, s;

	public:
		// x and y should be normalized
		Slerp(const Vec &x, const Vec &y);
		Vec operator ()(typename Vec::ElementType t) const;
	};
}

#include "general math.inl"