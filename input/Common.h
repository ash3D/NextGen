/*
\author		Arseny Lezin aka SleepWalk
\date		2.8.2015 (c)Arseny Lezin

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#if defined(_MSC_VER)
#	define _CRT_SECURE_NO_WARNINGS
#endif

#include "DGLE.h"

#include <string>
#include <vector>

#include "utils.h"

#include <dinput.h>
#include <dinputd.h>

#pragma message("Linking with dxguid.lib")
#pragma comment(linker, "/defaultlib:dxguid.lib")

#pragma message("Linking with dinput8.lib")
#pragma comment(linker, "/defaultlib:dinput8.lib")

using namespace DGLE;

#define PLUGIN_NAME				"DirectInput plugin"
#define PLUGIN_VERSION			("1.0 (" + std::string(__DATE__) + ")").c_str()
#define PLUGIN_VENDOR			"DeeProSoft"
#define PLUGIN_DISCRIPTION		"Change input subsystem on DirectInput."
#define PLUGIN_INTERFACE_NAME	"ISubSystemPlugin"

void LogWrite(uint uiInstIdx, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber);
inline void LogWrite(uint uiInstIdx, const std::string &str, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber)
{
	LogWrite(uiInstIdx, str.c_str(), eType, pcSrcFileName, iSrcLineNumber);
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
inline constexpr const char *const ExtractFilename(const char(&path)[length])
{
	static_assert(length > 0, "path must be null-terminated string");
	return detail::FindFilename<length - 1>(path);
}

#define LOG(txt, type) LogWrite(_uiInstIdx, txt, type, __FILE__, __LINE__)