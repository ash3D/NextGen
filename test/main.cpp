//==============================================================================================================================//
//																																//
// Attention! This project is for debugging engine purpose only. It's not a tutorial nor sample it's only for engine developers.//
// You can do testing for any feature which you are working on here. And please do not commit this file!						//
//																																//
//==============================================================================================================================//

#include "DGLE.h"
#include <string>

#define FORCE_NVIDIA_GPU

using namespace DGLE;

DGLE_DYNAMIC_FUNC

#define DLL_PATH "DGLE.dll"
#define APP_CAPTION "DevTest"
#define SCREEN_X 800
#define SCREEN_Y 600

#define INPUT_PLUGIN_PATH "..\\bin\\DirectInput.dll"
#define RENDER_PLUGIN_PATH "..\\bin\\CoreRendererDX9.dll"

IEngineCore *pEngineCore = NULL;
IInput *pInput = NULL;
IBitmapFont *pFnt = NULL;

TMouseStates stMouse;
TJoystickStates *stJoys;

bool bPressed[256];
bool bPressedEsc;
bool bPressedSpace;

uint uiJoyCnt;
uint uiCharCnt = 256;

char **ppcJoyName;

#ifdef FORCE_NVIDIA_GPU
extern "C" _declspec(dllexport) const DWORD NvOptimusEnablement = 0x00000001;
#endif

std::string IntToStr(int val)
{
	char res[16];
	sprintf_s(res, "%d", val);
	return std::string(res);
}

std::string UIntToStr(uint val)
{
	char res[32];
	sprintf_s(res, "%u", val);
	return std::string(res);
}

void KeyboardTest()
{
	uint y = 0;

	pFnt->Draw2DSimple(SCREEN_X - 100, 0, "Keyboard test: ", TColor4());

	for (uint i = 0; i < 256; ++i)
		if (bPressed[i])
			pFnt->Draw2DSimple(SCREEN_X - 100, y += 12, (IntToStr(i) + " pressed").c_str(), TColor4());
}

void MouseTest()
{
	uint y = 0;
	
	std::string strTemp = "Mouse coordinate X:" + IntToStr(stMouse.iX) + " Y: " + IntToStr(stMouse.iY);
	
	pFnt->Draw2DSimple(0 , 0, strTemp.c_str(), TColor4());

	if(stMouse.bLeftButton)
		pFnt->Draw2DSimple(0 , y += 12, "LeftButton mouse is pressed", TColor4());
	else
		pFnt->Draw2DSimple(0 , y += 12, "LeftButton mouse is not pressed", TColor4());

	if(stMouse.bRightButton)
		pFnt->Draw2DSimple(0 , y += 12, "RightButton mouse is pressed", TColor4());
	else
		pFnt->Draw2DSimple(0 , y += 12, "RightButton mouse is not pressed", TColor4());

	if(stMouse.bMiddleButton)
		pFnt->Draw2DSimple(0 , y += 12, "MiddleButton mouse is pressed", TColor4());
	else
		pFnt->Draw2DSimple(0 , y += 12, "MiddleButton mouse is not pressed", TColor4());

	if(stMouse.iDeltaWheel > 0)
		pFnt->Draw2DSimple(0 , y += 12, "Wheel up", TColor4());
	else
		if(stMouse.iDeltaWheel < 0)
			pFnt->Draw2DSimple(0 , y += 12, "Wheel down", TColor4());
		else
			pFnt->Draw2DSimple(0 , y += 12, "Wheel static", TColor4());
}

void JoystickTest()
{
	for (uint i = 0; i < uiJoyCnt; ++i)
	{
		uint x = i * 200, y = SCREEN_Y - 10;
	
		std::string strJoyParam;

		strJoyParam = "iPOV: " + IntToStr(stJoys[i].iPOV);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str(), TColor4());

		strJoyParam = "iZAxes: " + IntToStr(stJoys[i].iZAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str(), TColor4());

		strJoyParam = "iYAxes: " + IntToStr(stJoys[i].iYAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str(), TColor4());

		strJoyParam = "iXAxes: " + IntToStr(stJoys[i].iXAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str(), TColor4());

		strJoyParam = "iRAxes: " + IntToStr(stJoys[i].iRAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str(), TColor4());

		strJoyParam = "iUAxes: " + IntToStr(stJoys[i].iUAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str(), TColor4());

		strJoyParam = "iVAxes: " + IntToStr(stJoys[i].iVAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str(), TColor4());

		strJoyParam = "uiBtnsCount: " + IntToStr(stJoys[i].uiButtonsCount);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str(), TColor4());

		for(uint j = 0; j < stJoys[i].uiButtonsCount; j++)
		{
			if(stJoys[i].bButtons[j])
				strJoyParam = "Button: " + IntToStr(j + 1) + " is pressed"; 
			else
				strJoyParam = "Button: " + IntToStr(j + 1) + " is not pressed"; 

			pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str(), TColor4());
		}

		pFnt->Draw2DSimple(x, y -= 24, ppcJoyName[i], TColor4());
	}	
}

void DGLE_API Init(void *pParameter)
{
	pEngineCore->GetSubSystem(ESS_INPUT, (IEngineSubSystem *&)pInput);

	IResourceManager *p_res_man;
	pEngineCore->GetSubSystem(ESS_RESOURCE_MANAGER, (IEngineSubSystem *&)p_res_man);
	p_res_man->GetDefaultResource(EOT_BITMAP_FONT, (IEngineBaseObject *&)pFnt);

	pInput->GetJoysticksCount(uiJoyCnt);

	stJoys = new TJoystickStates[uiJoyCnt];

	ppcJoyName = new char *[uiJoyCnt];

	for (uint i = 0; i < uiJoyCnt; ++i)
	{
		ppcJoyName[i] = new char[uiCharCnt];

		pInput->GetJoystickName(i, ppcJoyName[i], uiCharCnt);
	}
}

void DGLE_API Free(void *pParameter)
{
	for (uint i = 0; i < uiJoyCnt; ++i)
		delete[] ppcJoyName[uiCharCnt];

	delete[] stJoys;
}

void DGLE_API Update(void *pParameter)
{
	pInput->GetMouseStates(stMouse);

	for (uint i = 0; i < 256; ++i)
	{
		pInput->GetKey((E_KEYBOARD_KEY_CODES)i, bPressed[i]);
		pInput->GetKey(KEY_ESCAPE, bPressedEsc);
		pInput->GetKey(KEY_SPACE, bPressedSpace);
	}

	for (uint i = 0; i < uiJoyCnt; ++i)
		pInput->GetJoystickStates(i, stJoys[i]);

	if(bPressedEsc)
		pEngineCore->QuitEngine();

	if(bPressed[2])
		pInput->Configure(ICF_DEFAULT);

	if(bPressed[3])
		pInput->Configure(ICF_EXCLUSIVE);

	if(bPressed[4])
		pInput->Configure(ICF_HIDE_CURSOR);

	if(bPressed[5])
		pInput->Configure(ICF_CURSOR_BEYOND_SCREEN);
}

void DGLE_API Render(void *pParameter)
{
	KeyboardTest();
	MouseTest();
	JoystickTest();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	if (GetEngine(DLL_PATH, pEngineCore))
	{
		pEngineCore->AddPluginToInitializationList(INPUT_PLUGIN_PATH);
		pEngineCore->AddPluginToInitializationList(RENDER_PLUGIN_PATH);

		if (SUCCEEDED(pEngineCore->InitializeEngine(NULL, APP_CAPTION, TEngineWindow(SCREEN_X, SCREEN_Y, false, false, MM_NONE, EWF_ALLOW_SIZEING), 33)))
		{
			pEngineCore->ConsoleVisible(true);
			pEngineCore->ConsoleExecute("core_fps_in_caption 1");
			
			pEngineCore->AddProcedure(EPT_INIT, &Init);
			pEngineCore->AddProcedure(EPT_FREE, &Free);
			pEngineCore->AddProcedure(EPT_UPDATE, &Update);
			pEngineCore->AddProcedure(EPT_RENDER, &Render);
			
			pEngineCore->StartEngine();
		}

		FreeEngine();
	}
	else
		MessageBox(NULL, "Couldn't load \""DLL_PATH"\"!", APP_CAPTION, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);

	return 0;
}