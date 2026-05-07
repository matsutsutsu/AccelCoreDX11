#include <windows.h>
#include <memory>
#include <assert.h>
#include <tchar.h>
#include <time.h>

#include "Framework.h"


//#include "PxPhysicsAPI.h"
//#pragma comment(lib, "PhysX_64.lib")
//#pragma comment(lib, "PhysXCommon_64.lib")
//#pragma comment(lib, "PhysXCooking_64.lib")
//#pragma comment(lib, "PhysXExtensions_static_64.lib")
//#pragma comment(lib, "PhysXFoundation_64.lib")
//#pragma comment(lib, "PhysXPvdSDK_static_64.lib")
//#pragma comment(lib, "PhysXTask_static_64.lib")
//#pragma comment(lib, "SceneQuery_static_64.lib")
//#pragma comment(lib, "SimulationController_static_64.lib")


#define SCREEN_MODE false // true: フルスクリーン、false: ウィンドウモード


const LONG SCREEN_WIDTH = 1920;
const LONG SCREEN_HEIGHT = 1080;
//const LONG SCREEN_WIDTH = 1280;
//const LONG SCREEN_HEIGHT = 720;

#if SCREEN_MODE
const BOOL FULLSCREEN = TRUE;
#else
const BOOL FULLSCREEN = FALSE;
#endif 

LRESULT CALLBACK fnWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	Framework *f = reinterpret_cast<Framework*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	return f ? f->HandleMessage(hwnd, msg, wparam, lparam) : DefWindowProc(hwnd, msg, wparam, lparam);
}




INT WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance, LPWSTR cmd_line, INT cmd_show)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(237);
#endif
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = fnWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = instance;
	wcex.hIcon = 0;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = _T("Game");
	wcex.hIconSm = 0;
	RegisterClassEx(&wcex);

    srand(static_cast<unsigned int>(time(nullptr)));

	HWND hWnd;
	if (FULLSCREEN)
	{
		DEVMODE devMode = {};
		devMode.dmSize = sizeof(DEVMODE);
		devMode.dmPelsWidth = SCREEN_WIDTH;
		devMode.dmPelsHeight = SCREEN_HEIGHT;
		devMode.dmBitsPerPel = 32;
		devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

		ChangeDisplaySettings(&devMode, CDS_FULLSCREEN);

		DWORD style = WS_POPUP | WS_VISIBLE;
		DWORD exStyle = 0; // WS_EX_TOPMOSTは付けない！

		hWnd = CreateWindowEx(exStyle, _T("Game"), _T("RuneForge"), style,
			0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
			NULL, NULL, instance, NULL);
	}
	else
	{
		RECT rc = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
		hWnd = CreateWindow(
			_T("Game"),
			_T("Game"),        // ここに表示したい文字を入れる
			WS_OVERLAPPEDWINDOW | WS_VISIBLE, // 標準的なウィンドウ形式に固定
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			rc.right - rc.left,
			rc.bottom - rc.top,
			NULL,
			NULL,
			instance,
			NULL
		);
	}

	Framework f(hWnd);
	SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&f));
	return f.Run();
}
