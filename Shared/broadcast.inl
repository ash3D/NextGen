/**
\author		Alexey Shaydurov aka ASH
\date		17.10.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "broadcast.h"
#include <utility>

#pragma region CCallbackHandle impl
template<typename ...Params>
inline void swap(typename CBroadcast<Params...>::CCallbackHandle &left, typename CBroadcast<Params...>::CCallbackHandle &right) noexcept
{
	std::swap(left._parent, right._parent);
	std::swap(left._iter, right._iter);
}

template<typename ...Params>
inline CBroadcast<Params...>::CCallbackHandle::CCallbackHandle(nullptr_t) noexcept :
_parent(), _iter()
{}

template<typename ...Params>
inline CBroadcast<Params...>::CCallbackHandle::CCallbackHandle(CBroadcast *parent, decltype(_iter) iter) noexcept :
	_parent(parent), _iter(iter)
{}

template<typename ...Params>
inline CBroadcast<Params...>::CCallbackHandle::CCallbackHandle(CCallbackHandle &&src) noexcept :
	_parent(src._parent), _iter(std::move(src._iter))
{
	src._parent = nullptr;
}

template<typename ...Params>
inline auto CBroadcast<Params...>::CCallbackHandle::operator =(CCallbackHandle &&src) noexcept -> CCallbackHandle &
{
	/*
	'::' and explicit template parameters required in some cases for VS 2013/2015
	it looks like overload resolution / template parameter deduction problem
	try with other compilers
	*/
	::swap<Params...>(*this, src);
	return *this;
}

template<typename ...Params>
CBroadcast<Params...>::CCallbackHandle::~CCallbackHandle()
{
	if (_parent)
		_parent->_callbacks.erase(_iter);
}
#pragma endregion

#pragma region CBroadcast impl
template<typename ...Params>
template<typename ...Args>
auto CBroadcast<Params...>::AddCallback(Args &&...args) -> CCallbackHandle
{
	_callbacks.emplace_front(std::forward<Args>(args)...);
	return CCallbackHandle(this, _callbacks.begin());
}

template<typename ...Params>
void CBroadcast<Params...>::operator ()(Params ...params) const
{
#ifdef ALLOW_CALLBACK_ERASE_DURING_CALL
	// this version allows for deletion callback being called
	auto cur = _callbacks.cbegin();
	const auto end = _callbacks.cend();
	while (cur != end)
		cur++->operator ()(params...);
#else
	for (auto &&callback : _callbacks)
		callback(params...);
#endif
}
#pragma endregion