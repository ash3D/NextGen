/**
\author		Alexey Shaydurov aka ASH
\date		02.01.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "Common.h"
#include "PluginCore.h"

static std::vector<std::unique_ptr<CPluginCore>> pluginCores;

void LogWrite(IEngineCore &engineCore, const char *pcTxt, E_LOG_TYPE eType, const char *pcSrcFileName, int iSrcLineNumber)
{
	AssertHR(engineCore.WriteToLogEx(pcTxt, eType, pcSrcFileName, iSrcLineNumber));
}

void CALLBACK InitPlugin(IEngineCore *engineCore, ISubSystemPlugin *&plugin)
{
	pluginCores.push_back(std::make_unique<CPluginCore>(engineCore));
	plugin = pluginCores.back().get();
}

void CALLBACK FreePlugin(IPlugin *plugin)
{
	typedef decltype(pluginCores) TPluginCores;
	pluginCores.erase(find_if(pluginCores.begin(), pluginCores.end(), [plugin](TPluginCores::const_reference curPlugin)
	{
		return curPlugin.get() == plugin;
	}));
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}