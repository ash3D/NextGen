/*
\author		Arseny Lezin aka SleepWalk
\date		31.7.2015 (c)Arseny Lezin

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
#include <filesystem>

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

#define LOG(txt, type) LogWrite(_uiInstIdx, txt, type, std::tr2::sys::path(__FILE__).filename().string().c_str(), __LINE__)