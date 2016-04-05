/**
\author		Alexey Shaydurov aka ASH
\date		01.04.2016 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#if !defined  __clang__  && defined _MSC_FULL_VER && _MSC_FULL_VER < 190023918
#error Old MSVC compiler version. Visual Studio 2015 Update 2 or later required.
#endif

#include "DGLE.h"
#include "DGLE_CoreRenderer.h"

#include "broadcast.h"
#include <string>
#include <functional>
#include <utility>
#include <type_traits>
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
	return std::hash<std::decay_t<T>>()(std::forward<T>(value));
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

void LogWrite(IEngineCore &engineCore, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber);
inline void LogWrite(IEngineCore &engineCore, const std::string &str, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber)
{
	LogWrite(engineCore, str.c_str(), eType, pcSrcFileName, iSrcLineNumber);
}

namespace detail
{
	template<size_t offset>
	inline constexpr const char *const FindFilename(const char path[])
	{
		return path[offset] == '\\' || path[offset] == '/' ? path + offset + 1 : FindFilename<offset - 1>(path);
	}

	template<>
	inline constexpr const char *const FindFilename<0>(const char path[])
	{
		return path;
	}
}

// use C++14 extended constexpr
template<size_t length>
inline constexpr const char *const ExtractFilename(const char (&path)[length])
{
	static_assert(length > 0, "path must be null-terminated string");
	return detail::FindFilename<length - 1>(path);
}

#define PTHIS(cl_name) (reinterpret_cast<cl_name *>(pParameter))
#define LOG(txt, type) LogWrite(_engineCore, txt, type, ExtractFilename(__FILE__), __LINE__)