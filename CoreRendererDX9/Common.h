/**
\author		Alexey Shaydurov aka ASH
\date		24.4.2015 (c)Korotkov Andrey

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
#include <unordered_map>
#include <cassert>

#include <wrl.h>

using namespace DGLE;

extern struct TEngInstance
{
	E_GET_ENGINE_FLAGS eGetEngFlags;
	class CConsole *pclConsole;
	class CCore *pclCore;

	TEngInstance() :
		eGetEngFlags(GEF_DEFAULT),
		pclConsole(NULL),
		pclCore(NULL)
	{}
} *EngineInstance(uint uiInstIdx);

class CInstancedObj
{
	const uint _instIdx;

protected:

	CInstancedObj(uint uiInstIdx) : _instIdx(uiInstIdx) {}

public:

	inline uint InstIdx() const
	{
		return _instIdx;
	}

	inline class CConsole *Console() const
	{
		return EngineInstance(_instIdx)->pclConsole;
	}

	inline class CCore *Core() const
	{
		return EngineInstance(_instIdx)->pclCore;
	}
};

inline void AssertHR(const HRESULT hr)
{
	assert(SUCCEEDED(hr));
}

#define PLUGIN_NAME				"CoreRendererDX9"
#define PLUGIN_VERSION			("0.1 (" + std::string(__DATE__) + ")").c_str()
#define PLUGIN_VENDOR			"DGLE Team"
#define PLUGIN_DESCRIPTION		"DirectX 9 implementation for ICoreRendererInterface"
#define PLUGIN_INTERFACE_NAME	"ISubSystemPlugin"

void LogWrite(uint uiInstIdx, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber);

#define LOG(txt, type) LogWrite(_uiInstIdx, std::string(txt).c_str(), type, GetFileName(__FILE__).c_str(), __LINE__)