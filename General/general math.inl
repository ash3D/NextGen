#pragma once

#include "general math.h"

namespace Math
{
	template<class T>
	Slerp<T>::Slerp(const T &x, const T &y): x(x), y(y)
	{
		nv_scalar c;
		dot(c, x, y);
		angle = c > -nv_one && c < +nv_one ? acos(c) : c >= +nv_one ? nv_zero : nv_pi;
		if (angle >= 1e-4f)
			s = nv_one / sin(angle);
	}

	template<class T>
	T Slerp<T>::operator ()(nv_scalar t) const
	{
		nv_scalar xfactor, yfactor;
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