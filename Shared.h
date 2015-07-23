/**
\author		Alexey Shaydurov aka ASH
\date		23.7.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "DGLE.h"
#include "DGLE_CoreRenderer.h"

#undef min
#undef max

#include <string>
#include <list>
#include <functional>
#include <utility>
#include <type_traits>
#include <filesystem>
#include <cassert>
#include <cstddef>

using namespace DGLE;

inline void AssertHR(const HRESULT hr)
{
	assert(SUCCEEDED(hr));
}

inline void CheckHR(const HRESULT hr)
{
	if (FAILED(hr)) throw hr;
}

#pragma region hash
template<typename T>
inline size_t HashValue(T &&value)
{
	return std::hash<std::decay<T>::type>()(std::forward<T>(value));
}

namespace detail
{
	template<typename Cur, typename ...Rest>
	inline size_t HashRange(size_t seed, Cur &&cur, Rest &&...rest)
	{
		return HashRange(seed ^ (HashValue(std::forward<Cur>(cur)) + 0x9e3779b9 + (seed << 6) + (seed >> 2)), std::forward<Rest>(rest)...);
	}

	inline size_t HashRange(size_t seed)
	{
		return seed;
	}
}

template<typename ...Args>
inline size_t HashRange(Args &&...args)
{
	return detail::HashRange(0, std::forward<Args>(args)...);
}
#pragma endregion consider moving it to utils or dedicated repo

#pragma region broadcast
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
	// enable_if - workaround for VS 2013/2015
	typename std::enable_if<true, decltype(_callbacks)>::type::const_iterator _iter;

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
	'::' and explicit template parameters required in some cases for VS 2013
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
#pragma endregion consider moving it to utils or dedicated repo

void LogWrite(IEngineCore &engineCore, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber);

#define PTHIS(cl_name) (reinterpret_cast<cl_name *>(pParameter))
#define LOG(txt, type) LogWrite(_engineCore, std::string(txt).c_str(), type, std::tr2::sys::path(__FILE__).filename().string().c_str(), __LINE__)