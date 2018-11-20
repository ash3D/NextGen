#pragma once

#include "gauss.h"
#include <cassert>

template<typename T>
inline void Math::Gauss::CGaussRange<T>::CIterator::_Check(CIterator iter1, CIterator iter2)
{
	assert(iter1._sigma == iter2._sigma);
}

template<typename T>
inline auto Math::Gauss::CGaussRange<T>::CIterator::operator *() const -> reference
{
	return Gauss(T(_x), _sigma);
}

template<typename T>
inline bool Math::Gauss::CGaussRange<T>::CIterator::operator ==(CIterator cmp) const
{
	_Check(*this, cmp);
	return _x == cmp._x;
}

template<typename T>
inline bool Math::Gauss::CGaussRange<T>::CIterator::operator !=(CIterator cmp) const
{
	return !operator ==(cmp);
}

template<typename T>
inline bool Math::Gauss::CGaussRange<T>::CIterator::operator <(CIterator cmp) const
{
	_Check(*this, cmp);
	return _x < cmp._x;
}

template<typename T>
inline auto Math::Gauss::CGaussRange<T>::CIterator::operator +=(difference_type offset) -> CIterator &
{
	_x += offset;
	return *this;
}

template<typename T>
inline auto Math::Gauss::CGaussRange<T>::CIterator::operator -=(difference_type offset) -> CIterator &
{
	_x -= offset;
	return *this;
}

template<typename T>
inline auto Math::Gauss::CGaussRange<T>::CIterator::operator ++() -> CIterator &
{
	return operator +=(1);
}

template<typename T>
inline auto Math::Gauss::CGaussRange<T>::CIterator::operator --() -> CIterator &
{
	return operator -=(1);
}

template<typename T>
inline auto Math::Gauss::CGaussRange<T>::CIterator::operator ++(int) -> CIterator
{
	CIterator old(*this);
	operator ++();
	return old;
}

template<typename T>
inline auto Math::Gauss::CGaussRange<T>::CIterator::operator --(int) -> CIterator
{
	CIterator old(*this);
	operator --();
	return old;
}

template<typename T>
inline auto Math::Gauss::CGaussRange<T>::CIterator::operator +(difference_type offset) const -> CIterator
{
	return CIterator(*this) += offset;
}

template<typename T>
inline auto Math::Gauss::CGaussRange<T>::CIterator::operator -(CIterator subtrahend) const -> difference_type
{
	_Check(*this, subtrahend);
	return _x - subtrahend._x;
}

template<typename T>
inline Math::Gauss::CGaussRange<T>::CIterator::CIterator(T sigma, int x) : _sigma(sigma), _x(x) {}

template<typename T>
inline Math::Gauss::CGaussRange<T>::CGaussRange(T sigma) : _sigma(sigma) {}

template<typename T>
inline typename Math::Gauss::CGaussRange<T>::CIterator CGaussRange<T>::begin() const
{
	return CIterator(_sigma, floor(-3 * _sigma));
}

template<typename T>
inline typename Math::Gauss::CGaussRange<T>::CIterator CGaussRange<T>::end() const
{
	return CIterator(_sigma, 1);
}