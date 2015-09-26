/**
\author		Alexey Shaydurov aka ASH
\date		26.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <ostream>
#include <type_traits>

class CBinaryOstream
{
	std::ostream &_out;
public:
	CBinaryOstream(std::ostream &out) noexcept : _out(out) {}
public:
	operator std::ostream &() noexcept
	{
		return _out;
	}
public:
	template<typename T>
	CBinaryOstream &operator <<(const T &data)
	{
		static_assert(std::is_standard_layout<T>::value, "try to write non-standard layout data");
		_out.write((const char *)&data, sizeof data);
		return *this;
	}
};