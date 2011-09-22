// render prototype.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "render prototype.h"
#include <Unknwn.h>
#include "..\\..\\Include\\CPP\\DGLE2.h"
#include "LowLevelRenderer.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

using namespace DGLE2::LowLevelRenderer;

HWND hWnd;

IDevice *device;
IDeviceContext *context;

std::vector<_2D::IEllipse *> ellipses;
struct TRectDesc
{
	TRectDesc(float x, float y, float width, float height, unsigned long color, float angle):
		x(x), y(y), width(width), height(height), color(color), angle(angle)
	{
	}
	float x, y, width, height;
	unsigned long color;
	float angle;
};
std::vector<TRectDesc> rects;

const unsigned count = 512;

void Proc()
{
	static float angle;
	//context->DrawRect(200, 200, 100, 100, ~0, angle);
	//for (unsigned i = 0; i < 1024; i++)
	//	context->DrawEllipse(320, 240, 300, 200, ~0, true, angle);
	//for (unsigned y = 0; y < count; y++)
	//	for (unsigned x = 0; x < count; x++)
	//		//context->DrawEllipse(800 * x / count + 800 / (count * 2), 600 * y / count + 600 / (count * 2), 800 / (count * 2), 600 / (count * 2), ~0, false, 0);
	//		context->DrawRect(800 * x / count + 800 / (count * 2), 600 * y / count + 600 / (count * 2), 800 / (count * 2), 600 / (count * 2), ~0, 0);
	std::for_each(rects.begin(), rects.end(), [=](const TRectDesc &rect){context->DrawRect(rect.x, rect.y, rect.width, rect.height, rect.color, rect.angle);});
	angle += 1e-2f;
	context->test();
	device->test();
	static unsigned fps;
	fps++;
	DWORD cur_tick = GetTickCount();
	static DWORD start_tick = cur_tick;
	if (cur_tick - start_tick >= 1000)
	{
		start_tick = cur_tick;
#define STR " fps"
		char str[std::numeric_limits<decltype(fps)>::digits10 + sizeof STR];
		sprintf_s(str, "%u"STR, fps);
#undef STR
		SetWindowTextA(hWnd, str);
		fps = 0;
	}
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_RENDERPROTOTYPE, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_RENDERPROTOTYPE));

	// Main message loop:
	while (true)
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			switch (msg.message)
			{
			case WM_QUIT:
				delete device;
				delete context;
				std::for_each(ellipses.begin(), ellipses.end(), [](const _2D::IEllipse *ellipse){delete ellipse;});
				return (int) msg.wParam;
			default:
				if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		else
			Proc();
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RENDERPROTOTYPE));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_RENDERPROTOTYPE);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	for (unsigned y = 0; y < count; y++)
		for (unsigned x = 0; x < count; x++)
			rects.push_back(TRectDesc(800 * x / count + 800 / (count * 2), 600 * y / count + 600 / (count * 2), 800 / (count * 2), 600 / (count * 2), ~0, 0));
   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   const IDisplayModes &modes = GetDisplayModes();
   modes[0];
   device = CreateDevice(hWnd, 800, 600);
   context = device->GetDeviceContext();
   ellipses.reserve(count * count);
	//for (unsigned y = 0; y < count; y++)
	//	for (unsigned x = 0; x < count; x++)
	//		//ellipses.push_back(device->AddEllipse(false, 0, 800 * x / count + 800 / (count * 2), 600 * y / count + 600 / (count * 2), 800 / (count * 2), 600 / (count * 2), ~0, false));
	//		(device->AddRect(true, 0, 800 * x / count + 800 / (count * 2), 600 * y / count + 600 / (count * 2), 800 / (count * 2), 600 / (count * 2), ~0));
	//(device->AddRect(true, ~0, 300, 200, 200, 100, ~0));

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
