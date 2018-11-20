/**
\author		Alexey Shaydurov aka ASH
\date		15.05.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include <list>
#include <functional>
#include <type_traits>

template<typename ...Params>
class CBroadcast
{
	std::list<std::function<void(Params...)>> _callbacks;

public:
	CBroadcast() = default;

	// disable copy/move in order to CCallbackHandle be valid during CBroadcast lifetime
	CBroadcast(CBroadcast &) = delete;
	void operator =(CBroadcast &) = delete;

public:
	class CCallbackHandle;

	template<typename ...Args>
	CCallbackHandle AddCallback(Args &&...args);

	void operator ()(Params ...params) const;
};

template<typename ...Params>
void swap(typename CBroadcast<Params...>::CCallbackHandle &left, typename CBroadcast<Params...>::CCallbackHandle &right) noexcept;

template<typename ...Params>
class CBroadcast<Params...>::CCallbackHandle final
{
	// try with different compilers
#if 0
	template<typename ...Args>
	friend CCallbackHandle CBroadcast::AddCallback(Args &&...args);
#else
	friend class CBroadcast;
#endif
#if 0
	friend void swap<>(typename CBroadcast<Params...>::CCallbackHandle &left, typename CBroadcast<Params...>::CCallbackHandle &right) noexcept;
#else
	template<typename ...Params>
	friend void swap(typename CBroadcast<Params...>::CCallbackHandle &left, typename CBroadcast<Params...>::CCallbackHandle &right) noexcept;
#endif

private:
	CBroadcast *_parent;
	typename decltype(_callbacks)::const_iterator _iter;

private:
	inline CCallbackHandle(CBroadcast *parent, decltype(_iter) iter) noexcept;

public:
	inline CCallbackHandle(nullptr_t = nullptr) noexcept;
	inline CCallbackHandle(CCallbackHandle &&src) noexcept;
	inline CCallbackHandle &operator =(CCallbackHandle &&src) noexcept;
	~CCallbackHandle();

public:
	CBroadcast *GetParent() const noexcept { return _parent; }
};

#include "broadcast.inl"