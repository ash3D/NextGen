/**
\author		Alexey Shaydurov aka ASH
\date		15.6.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "DGLE.h"
#include "DGLE_CoreRenderer.h"
#include "utils.h"

#undef min
#undef max

#include <memory>
#include <string>
#include <array>
#include <list>
#include <unordered_map>
#include <functional>
#include <utility>
#include <type_traits>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <cmath>

#include <wrl.h>

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
inline size_t GetHash(T &&value)
{
	return std::hash<std::decay<T>::type>()(std::forward<T>(value));
}

namespace detail
{
	template<typename Cur, typename ...Rest>
	inline size_t ComposeHash(size_t accum, Cur &&cur, Rest &&...rest)
	{
		return ComposeHash(accum * 31 + GetHash(std::forward<Cur>(cur)), std::forward<Rest>(rest)...);
	}

	inline size_t ComposeHash(size_t accum)
	{
		return accum;
	}
}

template<typename ...Args>
inline size_t ComposeHash(Args &&...args)
{
	return detail::ComposeHash(17, std::forward<Args>(args)...);
}
#pragma endregion consider moving it to utils or dedicated repo

#pragma region broadcast
template<typename ...Params>
class CBroadcast
{
	std::list<std::function<void (Params...)>> _callbacks;

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
void swap(typename CBroadcast<Params...>::CCallbackHandle &left, typename CBroadcast<Params...>::CCallbackHandle &right) /*noexcept*/;

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
	friend void swap<>(typename CBroadcast<Params...>::CCallbackHandle &left, typename CBroadcast<Params...>::CCallbackHandle &right) /*noexcept*/;
#else
	template<typename ...Params>
	friend void swap(typename CBroadcast<Params...>::CCallbackHandle &left, typename CBroadcast<Params...>::CCallbackHandle &right) /*noexcept*/;
#endif

private:
	CBroadcast *_parent = nullptr;
	typename std::enable_if<true, decltype(_callbacks)>::type::const_iterator _iter{};

private:
	inline CCallbackHandle(CBroadcast *parent, decltype(_iter) iter) /*noexcept*/;

public:
	inline CCallbackHandle() = default;
	inline CCallbackHandle(CCallbackHandle &&src) /*noexcept*/;
	inline CCallbackHandle &operator =(CCallbackHandle &&src) /*noexcept*/;
	~CCallbackHandle();

public:
	CBroadcast *GetParent() const /*noexcept*/ { return _parent; }
};

#pragma region CCallbackHandle impl
template<typename ...Params>
inline void swap(typename CBroadcast<Params...>::CCallbackHandle &left, typename CBroadcast<Params...>::CCallbackHandle &right) /*noexcept*/
{
	std::swap(left._parent, right._parent);
	std::swap(left._iter, right._iter);
}

template<typename ...Params>
inline CBroadcast<Params...>::CCallbackHandle::CCallbackHandle(CBroadcast *parent, decltype(_iter) iter) /*noexcept*/ :
_parent(parent), _iter(iter)
{}

template<typename ...Params>
inline CBroadcast<Params...>::CCallbackHandle::CCallbackHandle(CCallbackHandle &&src) /*noexcept*/ :
_parent(src._parent), _iter(std::move(src._iter))
{
	src._parent = nullptr;
}

template<typename ...Params>
inline auto CBroadcast<Params...>::CCallbackHandle::operator =(CCallbackHandle &&src) -> CCallbackHandle & /*noexcept*/
{
	swap(*this, src);
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
	_callbacks.emplace_front(forward<Args>(args)...);
	return CCallbackHandle(this, _callbacks.begin());
}

template<typename ...Params>
void CBroadcast<Params...>::operator ()(Params ...params) const
{
	for (auto &&callback : _callbacks)
		callback(params...);
}
#pragma endregion
#pragma endregion consider moving it to utils or dedicated repo

#define PLUGIN_NAME				"CoreRendererDX9"
#define PLUGIN_VERSION			("0.1 (" + std::string(__DATE__) + ")").c_str()
#define PLUGIN_VENDOR			"DGLE Team"
#define PLUGIN_DESCRIPTION		"ICoreRendererInterface DirectX 9 implementation"
#define PLUGIN_INTERFACE_NAME	"ISubSystemPlugin"

void LogWrite(IEngineCore &engineCore, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber);

#define PTHIS(cl_name) (reinterpret_cast<cl_name *>(pParameter))
#define LOG(txt, type) LogWrite(_engineCore, std::string(txt).c_str(), type, GetFileName(__FILE__).c_str(), __LINE__)