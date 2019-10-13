#pragma once

#define _USE_MATH_DEFINES
#include "general math.h"
#include <cmath>

namespace Math
{
	template<class Vec>
	Slerp<Vec>::Slerp(const Vec &x, const Vec &y): x(x), y(y)
	{
		// rely on ADL
		const auto c = dot(x, y);
		angle = c > -1 && c < +1 ? acos(c) : c >= +1 ? 0 : M_PI;
		if (angle >= 1e-4f)
			s = 1 / sin(angle);
	}

	template<class Vec>
	Vec Slerp<Vec>::operator ()(typename Vec::ElementType t) const
	{
		typename Vec::ElementType xfactor, yfactor;
		if (angle < 1e-4f)
		{
			xfactor = 1 - t;
			yfactor = t;
		}
		else
		{
			xfactor = sin((1 - t) * angle) * s;
			yfactor = sin(t * angle) * s;
		}
		return xfactor * x + yfactor * y;
	}
}