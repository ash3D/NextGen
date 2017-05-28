//==============================================================================================================================//
//																																//
// Attention! This project is for debugging engine purpose only. It's not a tutorial nor sample it's only for engine developers.//
// You can do testing for any feature which you are working on here. And please do not commit this file!						//
//																																//
//==============================================================================================================================//

#include "DGLE.h"
#include <string>
#include <memory>
#include <type_traits>

#define FORCE_NVIDIA_GPU 1

using namespace DGLE;

DGLE_DYNAMIC_FUNC

#define DLL_PATH "DGLE.dll"
static constexpr const char appCaption[] = "DevTest";
static constexpr unsigned int screenRes[2] = { 800, 600 };

static constexpr const char inputPluginPath[] = "..\\bin\\DirectInput.dll", renderPluginPath[] = "..\\bin\\CoreRendererDX9.dll";

static IEngineCore *pEngineCore;
static IInput *pInput;
static IBitmapFont *pFnt;

static TMouseStates stMouse;
static TJoystickStates *stJoys;

static bool bPressed[256];
static bool bPressedEsc;
static bool bPressedSpace;

static uint uiJoyCnt;

static std::unique_ptr<char []> *ppcJoyName;

using std::to_string;

#if FORCE_NVIDIA_GPU
extern "C" _declspec(dllexport) const DWORD NvOptimusEnablement = 0x00000001;
#endif

void KeyboardTest()
{
	uint y = 250;

	pFnt->Draw2DSimple(screenRes[0] - 100, y, "Keyboard test: ");

	for (uint i = 0; i < 256; ++i)
		if (bPressed[i])
			pFnt->Draw2DSimple(screenRes[0] - 100, y += 12, (to_string(i) + " pressed").c_str());
}

void MouseTest()
{
	uint y = 250;
	
	std::string strTemp = "Mouse coordinate X:" + to_string(stMouse.iX) + " Y: " + to_string(stMouse.iY);
	
	pFnt->Draw2DSimple(0 , y, strTemp.c_str());

	if(stMouse.bLeftButton)
		pFnt->Draw2DSimple(0 , y += 12, "LeftButton mouse is pressed");
	else
		pFnt->Draw2DSimple(0 , y += 12, "LeftButton mouse is not pressed");

	if(stMouse.bRightButton)
		pFnt->Draw2DSimple(0 , y += 12, "RightButton mouse is pressed");
	else
		pFnt->Draw2DSimple(0 , y += 12, "RightButton mouse is not pressed");

	if(stMouse.bMiddleButton)
		pFnt->Draw2DSimple(0 , y += 12, "MiddleButton mouse is pressed");
	else
		pFnt->Draw2DSimple(0 , y += 12, "MiddleButton mouse is not pressed");

	if(stMouse.iDeltaWheel > 0)
		pFnt->Draw2DSimple(0 , y += 12, "Wheel up");
	else
		if(stMouse.iDeltaWheel < 0)
			pFnt->Draw2DSimple(0 , y += 12, "Wheel down");
		else
			pFnt->Draw2DSimple(0 , y += 12, "Wheel static");
}

void JoystickTest()
{
	for (uint i = 0; i < uiJoyCnt; ++i)
	{
		uint x = i * 200, y = screenRes[1] - 10;
	
		std::string strJoyParam;

		strJoyParam = "iPOV: " + to_string(stJoys[i].iPOV);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str());

		strJoyParam = "iZAxes: " + to_string(stJoys[i].iZAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str());

		strJoyParam = "iYAxes: " + to_string(stJoys[i].iYAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str());

		strJoyParam = "iXAxes: " + to_string(stJoys[i].iXAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str());

		strJoyParam = "iRAxes: " + to_string(stJoys[i].iRAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str());

		strJoyParam = "iUAxes: " + to_string(stJoys[i].iUAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str());

		strJoyParam = "iVAxes: " + to_string(stJoys[i].iVAxis);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str());

		strJoyParam = "uiBtnsCount: " + to_string(stJoys[i].uiButtonsCount);
		pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str());

		for (uint j = 0; j < stJoys[i].uiButtonsCount; j++)
		{
			if (stJoys[i].bButtons[j])
				strJoyParam = "Button: " + to_string(j + 1) + " is pressed"; 
			else
				strJoyParam = "Button: " + to_string(j + 1) + " is not pressed"; 

			pFnt->Draw2DSimple(x, y -= 12, strJoyParam.c_str());
		}

		pFnt->Draw2DSimple(x, y -= 24, ppcJoyName[i].get());
	}	
}

void DGLE_API Init(void *pParameter)
{
	pEngineCore->GetSubSystem(ESS_INPUT, (IEngineSubSystem *&)pInput);

	IResourceManager *p_res_man;
	pEngineCore->GetSubSystem(ESS_RESOURCE_MANAGER, (IEngineSubSystem *&)p_res_man);
	p_res_man->GetDefaultResource(EOT_BITMAP_FONT, (IEngineBaseObject *&)pFnt);

	pInput->GetJoysticksCount(uiJoyCnt);

	stJoys = new TJoystickStates[uiJoyCnt]();

	ppcJoyName = new std::remove_pointer_t<decltype(ppcJoyName)>[uiJoyCnt];

	for (uint i = 0; i < uiJoyCnt; ++i)
	{
		uint uiCharCnt;
		pInput->GetJoystickName(i, NULL, uiCharCnt);
		ppcJoyName[i].reset(new char[uiCharCnt]);
		pInput->GetJoystickName(i, ppcJoyName[i].get(), uiCharCnt);
	}
}

void DGLE_API Free(void *pParameter)
{
	delete[] ppcJoyName;
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

	if (bPressedEsc)
		pEngineCore->QuitEngine();

	if (bPressed[2])
		pInput->Configure(ICF_DEFAULT);

	if (bPressed[3])
		pInput->Configure(ICF_EXCLUSIVE);

	if (bPressed[4])
		pInput->Configure(ICF_HIDE_CURSOR);

	if (bPressed[5])
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
		pEngineCore->AddPluginToInitializationList(inputPluginPath);
		pEngineCore->AddPluginToInitializationList(renderPluginPath);

		if (SUCCEEDED(pEngineCore->InitializeEngine(NULL, appCaption, TEngineWindow(screenRes[0], screenRes[1], false, false, MM_NONE, EWF_ALLOW_SIZEING), 33)))
		{
			pEngineCore->ConsoleVisible(true);
			pEngineCore->ConsoleExecute("core_fps_in_caption 1");
			pEngineCore->ConsoleExecute("crdx9_profiler 6");
			
			pEngineCore->AddProcedure(EPT_INIT, &Init);
			pEngineCore->AddProcedure(EPT_FREE, &Free);
			pEngineCore->AddProcedure(EPT_UPDATE, &Update);
			pEngineCore->AddProcedure(EPT_RENDER, &Render);
			
			pEngineCore->StartEngine();
		}

		FreeEngine();
	}
	else
		MessageBox(NULL, "Couldn't load \"" DLL_PATH "\"!", appCaption, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);

	return 0;
}