#pragma once

#include <cstdint>
#include "nv_algebra.h"	// for Slerp

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

	template<typename Left, typename Right, typename Param>
	inline auto lerp(const Left &left, const Right &right, const Param &param)
	{
		return (1 - param) * left + param * right;
	}

	template<class T>
	class Slerp
	{
	public:
		inline Slerp() {}
		// x and y should be normalized
		inline Slerp(const T &x, const T &y);
		inline T operator ()(nv_scalar t) const;
	private:
		T x, y;
		nv_scalar angle, s;
	};
}

#include "general math.inl"