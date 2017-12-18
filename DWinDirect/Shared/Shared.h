/**
\author		Alexey Shaydurov aka ASH
\date		18.12.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#if !defined  __clang__  && defined _MSC_VER && _MSC_VER < 1911
#error Old MSVC compiler version. Visual Studio 2017 15.3 or later required.
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

#if defined _MSC_VER && _MSC_VER <= 1912 && !defined __clang__
#	define MSVC_LIMITATIONS
#endif

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
	inline size_t HashRange(size_t seed)
	{
		return seed;
	}

	template<typename Cur, typename ...Rest>
	inline size_t HashRange(size_t seed, Cur &&cur, Rest &&...rest)
	{
		return HashRange(seed ^ (HashValue(std::forward<Cur>(cur)) + 0x9e3779b9 + (seed << 6) + (seed >> 2)), std::forward<Rest>(rest)...);
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

template<size_t length>
inline constexpr const char *const ExtractFilename(const char(&path)[length])
{
	static_assert(length > 0, "path must be null-terminated string");
	auto offset = length;
	do
		if (path[offset - 1] == '\\' || path[offset - 1] == '/')
			return path + offset;
	while (--offset);
	return path;
}

#define PTHIS(cl_name) (reinterpret_cast<cl_name *>(pParameter))
#define LOG(txt, type) LogWrite(_engineCore, txt, type, ExtractFilename(__FILE__), __LINE__)