/**
\author		Alexey Shaydurov aka ASH
\date		26.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <iterator>
#define _USE_MATH_DEFINES
#include <cmath>

namespace Math
{
	namespace Gauss
	{
		// normalized gaussian
		template<typename T>
		extern inline T Gauss(T x, T sigma, T a = 0)
		{
			auto dx = x - a;
			return exp(-(dx * dx) / (sigma * sigma)) / (sigma * 2 * M_2_SQRTPI);
		}

		// left half gaussian range
		template<typename T>
		class CGaussRange
		{
		public:
			class CIterator;
			CGaussRange(T sigma);
			CIterator begin() const;
			CIterator end() const;
		private:
			const T _sigma;
		};

		template<typename T>
		class CGaussRange<T>::CIterator : public std::iterator<std::random_access_iterator_tag, T, int, const T *, T>
		{
			friend class CGaussRange;
		public:
			reference operator *() const;
			void operator ->() const = delete;
			bool operator ==(CIterator cmp) const;
			bool operator !=(CIterator cmp) const;
			bool operator <(CIterator cmp) const;
			CIterator &operator +=(difference_type offset);
			CIterator &operator -=(difference_type offset);
			CIterator &operator ++();
			CIterator &operator --();
			CIterator operator ++(int);
			CIterator operator --(int);
			CIterator operator +(difference_type offset) const;
			difference_type operator -(CIterator subtrahend) const;
		private:
			CIterator(T sigma, int x);
			static void _Check(CIterator iter1, CIterator iter2);
			const T _sigma;
			int _x;
		};
	}
}

#include "gauss.inl"