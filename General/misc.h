/**
\author		Alexey Shaydurov aka ASH
\date		26.9.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <utility>	// for std::move
#include <type_traits>
#include <iterator>

#pragma region reserve
template<class Container, typename Iterator>
inline void Reserve(Container &container, Iterator begin, Iterator end);

template<bool reserve>
struct TReserveImpl;

template<>
struct TReserveImpl<false>
{
	template<class Container, typename Iterator>
	friend inline void Reserve(Container &container, Iterator begin, Iterator end);
private:
	template<class Container, typename Iterator>
	static inline void apply(Container &container, Iterator begin, Iterator end) {}
};

template<>
struct TReserveImpl<true>
{
	template<class Container, typename Iterator>
	friend inline void Reserve(Container &container, Iterator begin, Iterator end);
private:
	template<class Container, typename Iterator>
	static inline void apply(Container &container, Iterator begin, Iterator end)
	{
		container.reserve(end - begin);
	}
};

template<class Container, typename Iterator>
inline void Reserve(Container &container, Iterator begin, Iterator end)
{
	TReserveImpl<std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>::value>::apply(container, begin, end);
}
#pragma endregion

#pragma region mutable wrapper
// TODO: consider non-const assign operators (to preserve constless in return)
template<typename T>
struct TMutableWrapper
{
	TMutableWrapper() = default;
	TMutableWrapper(const T &src) : _value(src) {}
	TMutableWrapper(T &&src)/* = default*/ : _value(std::move(src)) {}
	TMutableWrapper(const TMutableWrapper &&src) : _value(std::move(src._value)) {}
	TMutableWrapper(const TMutableWrapper &) = default;
	const TMutableWrapper &operator =(const T &src) const
	{
		_value = src;
		return *this;
	}
	const TMutableWrapper &operator =(const T &&src) const
	{
		_value = std::move(src);
		return *this;
	}
	const TMutableWrapper &operator =(const TMutableWrapper &src) const
	{
		_value = src._value;
		return *this;
	}
	const TMutableWrapper &operator =(TMutableWrapper &&src) const
	{
		_value = std::move(src._value);
		return *this;
	}
	operator T &() const
	{
		return _value;
	}
private:
	mutable T _value;
};
#pragma endregion

#pragma region size_t wrapper
template<class Container>
struct TSizeTWrapper
{
private:
	template<class _Container>
	static typename _Container::size_type _GetSizeT(typename _Container::size_type);
	template<class>
	static size_t _GetSizeT(...);
public:
	typedef decltype(_GetSizeT<Container>(0)) type;
};
#pragma endregion