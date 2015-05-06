/**
\author		Alexey Shaydurov aka ASH
\date		7.5.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "PluginCore.h"

using namespace std;

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
	strcpy(stInfo.cName, PLUGIN_NAME);
	strcpy(stInfo.cVersion, PLUGIN_VERSION);
	strcpy(stInfo.cVendor, PLUGIN_VENDOR);
	strcpy(stInfo.cDescription, PLUGIN_DESCRIPTION);
	stInfo.ui8PluginSDKVersion = _DGLE_PLUGIN_SDK_VER_;
	
	return S_OK;
}

DGLE_RESULT DGLE_API CPluginCore::GetPluginInterfaceName(char *pcName, uint &uiCharsCount)
{
	if (!pcName)
	{
		uiCharsCount = (uint)strlen(PLUGIN_INTERFACE_NAME) + 1;
		return S_OK;	
	}
	
	if (uiCharsCount < (uint)strlen(PLUGIN_INTERFACE_NAME))
	{
		uiCharsCount = (uint)strlen(PLUGIN_INTERFACE_NAME) + 1;
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