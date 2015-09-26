/**
\author		Alexey Shaydurov aka ASH
\date		26.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "nv_algebra.h"	// for Slerp

namespace Math
{
	template<typename Left, typename Right, typename Param>
	inline auto lerp(const Left &left, const Right &right, const Param &param) -> decltype((1 - param) * left + param * right)
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