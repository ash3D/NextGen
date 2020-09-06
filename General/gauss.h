#pragma once

#include "CompilerCheck.h"
#include <iterator>
#include <cmath>
#include <numbers>

namespace Math::Gauss
{
	// normalized gaussian
	template<typename T>
	extern inline T Gauss(T x, T sigma, T a = 0)
	{
		using namespace std::numbers;
		const auto dx = x - a;
		return exp(dx * dx / (-2 * sigma * sigma)) * inv_sqrtpi_v<T> / (sqrt2_v<T> * sigma);
	}

	// left half gaussian range
	template<typename T>
	class CGaussRange
	{
		const T _sigma;
	public:
		class CIterator;
		CGaussRange(T sigma);
		CIterator begin() const;
		CIterator end() const;
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

#include "gauss.inl"