/**
\author		Alexey Shaydurov aka ASH
\date		5.7.2015 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "Common.h"
#include "CoreRendererDX11.h"

class CPluginCore : public ISubSystemPlugin
{
	friend void LogWrite(uint uiInstIdx, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber);

private:

	uint _instIdx;
	IEngineCore *_pEngineCore;
	//CCoreRendererDX11 _renderer;

public:

	CPluginCore(IEngineCore *pEngineCore);
	~CPluginCore();

	DGLE_RESULT DGLE_API GetPluginInfo(TPluginInfo &stInfo);
	DGLE_RESULT DGLE_API GetPluginInterfaceName(char *pcName, uint &uiCharsCount);

	DGLE_RESULT DGLE_API GetSubSystemInterface(IEngineSubSystem *&prSubSystem);

	IDGLE_BASE_IMPLEMENTATION(ISubSystemPlugin, INTERFACE_IMPL_END)
};