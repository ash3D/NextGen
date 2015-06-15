/*
\author		Arseny Lezin aka SleepWalk
\date		15.6.2015 (c)Arseny Lezin

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "Common.h"
#include "DirectInput.h"

class CPluginCore : public ISubSystemPlugin
{
	friend void LogWrite(uint uiInstIdx, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber);

private:
	
	uint				 _uiInstIdx;
	IEngineCore			*_pEngineCore;

	CDirectInput		*_pDirectInput;

public:

	CPluginCore(IEngineCore *pEngineCore);
	~CPluginCore();

	DGLE_RESULT DGLE_API GetPluginInfo(TPluginInfo &stInfo);
	DGLE_RESULT DGLE_API GetPluginInterfaceName(char* pcName, uint &uiCharsCount);

	DGLE_RESULT DGLE_API GetSubSystemInterface(IEngineSubSystem *&prSubSystem);
	
	IDGLE_BASE_IMPLEMENTATION(ISubSystemPlugin, INTERFACE_IMPL_END)
};