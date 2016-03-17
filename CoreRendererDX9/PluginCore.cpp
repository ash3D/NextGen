/**
\author		Alexey Shaydurov aka ASH
\date		17.03.2016 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "PluginCore.h"

#define PLUGIN_NAME				"CoreRendererDX9"
#define PLUGIN_VERSION			("0.1 (" + std::string(__DATE__) + ")").c_str()
#define PLUGIN_VENDOR			"DGLE Team"
#define PLUGIN_DESCRIPTION		"ICoreRendererInterface DirectX 9 implementation"
#define PLUGIN_INTERFACE_NAME	"ISubSystemPlugin"

static inline uint GetInstanceIndex(IEngineCore *pEngineCore)
{
	uint inst_idx;
	AssertHR(pEngineCore->GetInstanceIndex(inst_idx));
	return inst_idx;
}

CPluginCore::CPluginCore(IEngineCore *pEngineCore):
_instIdx(GetInstanceIndex(pEngineCore)),
_pEngineCore(pEngineCore), _renderer(*pEngineCore)
{
	assert(pEngineCore);
}

CPluginCore::~CPluginCore() = default;

DGLE_RESULT DGLE_API CPluginCore::GetPluginInfo(TPluginInfo &stInfo)
{
	using std::size;
	// ' - 1' for terminator
	strncpy(stInfo.cName, PLUGIN_NAME, size(stInfo.cName) - 1);
	strncpy(stInfo.cVersion, PLUGIN_VERSION, size(stInfo.cVersion) - 1);
	strncpy(stInfo.cVendor, PLUGIN_VENDOR, size(stInfo.cVendor) - 1);
	strncpy(stInfo.cDescription, PLUGIN_DESCRIPTION, size(stInfo.cDescription) - 1);
	stInfo.ui8PluginSDKVersion = _DGLE_PLUGIN_SDK_VER_;
	
	return S_OK;
}

DGLE_RESULT DGLE_API CPluginCore::GetPluginInterfaceName(char *pcName, uint &uiCharsCount)
{
	if (!pcName)
	{
		uiCharsCount = strlen(PLUGIN_INTERFACE_NAME) + 1;
		return S_OK;	
	}
	
	if (uiCharsCount < strlen(PLUGIN_INTERFACE_NAME))
	{
		uiCharsCount = strlen(PLUGIN_INTERFACE_NAME) + 1;
		strcpy(pcName, "");
		return E_INVALIDARG;
	}

	strcpy(pcName, PLUGIN_INTERFACE_NAME);
	
	return S_OK;
}

DGLE_RESULT DGLE_API CPluginCore::GetSubSystemInterface(IEngineSubSystem *&prSubSystem)
{
	prSubSystem = reinterpret_cast<IEngineSubSystem *const>(&_renderer);

	return S_OK;
}