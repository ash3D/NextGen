/**
\author		Alexey Shaydurov aka ASH
\date		13.6.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "DGLE.h"
#include "DGLE_CoreRenderer.h"

#include <memory>
#include <string>
#include <array>
#include <list>
#include <unordered_map>
#include <functional>
#include <utility>
#include <cassert>

#include <wrl.h>

#include "utils.h"

using namespace DGLE;

inline void AssertHR(const HRESULT hr)
{
	assert(SUCCEEDED(hr));
}

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
#define PLUGIN_DESCRIPTION		"DirectX 9 implementation for ICoreRendererInterface"
#define PLUGIN_INTERFACE_NAME	"ISubSystemPlugin"

void LogWrite(IEngineCore &engineCore, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber);

#define PTHIS(cl_name) (reinterpret_cast<cl_name *>(pParameter))
#define LOG(txt, type) LogWrite(_engineCore, std::string(txt).c_str(), type, GetFileName(__FILE__).c_str(), __LINE__)