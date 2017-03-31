// intercept.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Commctrl.h"
#include "resource.h"
#include <thread>
#include "key.h"
#include "util.h"
#ifdef UNICODE
#define stringcopy wcscpy
#else
#define stringcopy strcpy
#endif
#define ID_TRAY_APP_ICON                5000
#define ID_TRAY_EXIT_CONTEXT_MENU_ITEM  3000
#define WM_TRAYICON ( WM_USER + 1 )
#pragma endregion

#pragma region constants and globals
UINT WM_TASKBARCREATED = 0;

HWND g_hwnd;
HMENU g_menu;
HINSTANCE hInstance;

NOTIFYICONDATA g_notifyIconData;
#pragma endregion


LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

#pragma region helper funcs
void Minimize(void) {
	ShowWindow(g_hwnd, SW_HIDE);
}
void InitNotifyIconData()
{
	g_notifyIconData.uVersion = NOTIFYICON_VERSION_4;
	g_notifyIconData.hWnd = g_hwnd;
	stringcopy(g_notifyIconData.szTip, _T("Intercept"));
	g_notifyIconData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
	g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_notifyIconData.uCallbackMessage = WM_TRAYICON;
}
#pragma endregion
void help(void)
{
	printf("Command line parameters:\n");
	printf("\t/ini path\to\file.ini specify alternate config file (optional)\n");
	printf("\t/apply                non-interactive, apply filters on startup (optional)\n");
}

int _tmain(int argc, _TCHAR* argv[]) {
	TCHAR currDir[1024];
	GetCurrentDirectory(1024, currDir);
	CHAR iniFile[1024];
	sprintf(iniFile, "%s\\keyremap.ini", currDir);
	int mode_apply = 0;
	for (int i = 0; i < argc; i++) {
		if ((stricmp(argv[i], "/help") == 0) || (stricmp(argv[i], "/?") == 0)) {
			help();
			return 1;
		}

		if (stricmp(argv[i], "/apply") == 0)
			mode_apply = 1;

		if (stricmp(argv[i], "/ini") == 0) {
			// Missing parameter
			if (i == argc - 1)
				help();

			strcpy(iniFile, argv[i + 1]);
			i++;
		}
	}
	printf("Using configuration file %s\n\n", iniFile);
	KeyFilterSet kfs(iniFile);
	if (mode_apply) {
		TCHAR className[] = TEXT("Intercept");
		hInstance = GetModuleHandle(NULL);
		ShowWindow(GetConsoleWindow(), SW_HIDE);
		WM_TASKBARCREATED = RegisterWindowMessageA("TaskbarCreated");
#pragma region get window up
		WNDCLASSEX wnd = { 0 };
		wnd.hInstance = hInstance;
		wnd.lpszClassName = className;
		wnd.lpfnWndProc = WndProc;
		wnd.style = CS_HREDRAW | CS_VREDRAW;
		wnd.cbSize = sizeof(WNDCLASSEX);

		wnd.hIcon = LoadIcon(NULL, MAKEINTRESOURCE(IDI_ICON));
		wnd.hIconSm = LoadIcon(NULL, MAKEINTRESOURCE(IDI_ICON));
		wnd.hCursor = LoadCursor(NULL, IDC_ARROW);
		wnd.hbrBackground = (HBRUSH)COLOR_APPWORKSPACE;

		if (!RegisterClassEx(&wnd))
		{
			FatalAppExit(0, TEXT("Couldn't register window class!"));
		}

		g_hwnd = CreateWindowEx(
			0,
			className,
			TEXT("Intercept"),
			WS_MINIMIZE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			NULL,
			NULL,
			hInstance, NULL
		);
		Minimize();
		InitNotifyIconData();
		Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
#pragma endregion
		std::thread t(&KeyFilterSet::Run, kfs);
		t.detach();
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData);
		t.~thread();
	}
	else {
		while (1) {
			char c = SelectOption("(L)ist filters, (S)how/(A)dd/(R)emove filter, appl(Y) filters or (Q)uit?", "lsaryq");
			switch (c) {
			case 'l':
				kfs.Print();
				break;

			case 's':
				kfs.Print(kfs.GetFilterNumber());
				break;

			case 'q':
				if (SelectOption("(R)eally quit, or (G)o back?", "rg") == 'r')
					return 0;
				break;

			case 'a':
				kfs.AddNew();
				break;

			case 'r':
				kfs.Delete(kfs.GetFilterNumber());
				break;

			case 'y':
				printf("\n\nKeyboard filters activated.");
				printf("\nPlease close this window to restore normal behavior.");
				printf("\nTo activate filters on startup, add /apply to the command line.\n");
				kfs.Run();
				break;
			}
		}
	}
	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_TASKBARCREATED && !IsWindowVisible(g_hwnd))
	{
		Minimize();
		return 0;
	}
	switch (message)
	{
	case WM_CREATE:
		printf("CREATION MESSAGE");
		g_menu = CreatePopupMenu();
		AppendMenu(g_menu, MF_STRING, ID_TRAY_EXIT_CONTEXT_MENU_ITEM, TEXT("Exit Intercept"));
		break;
	case WM_SYSCOMMAND:
		switch (wParam & 0xfff0)  // (filter out reserved lower 4 bits:  see msdn remarks http://msdn.microsoft.com/en-us/library/ms646360(VS.85).aspx)
		{
		case SC_MINIMIZE:
		case SC_CLOSE:  // redundant to WM_CLOSE, it appears
			Minimize();
			return 0;
			break;
		}
		break;
		// Our user defined WM_TRAYICON message.
	case WM_TRAYICON:
		if (lParam == WM_RBUTTONDOWN)
		{
			POINT curPoint;
			GetCursorPos(&curPoint);

			SetForegroundWindow(hwnd);
			UINT clicked = TrackPopupMenu(

				g_menu,
				TPM_RETURNCMD | TPM_NONOTIFY, // don't send me WM_COMMAND messages about this window, instead return the identifier of the clicked menu item
				curPoint.x,
				curPoint.y,
				0,
				hwnd,
				NULL

			);
			if (clicked == ID_TRAY_EXIT_CONTEXT_MENU_ITEM)
			{
				PostQuitMessage(0);
			}
		}

		break;
	case WM_NCHITTEST:
	{
		// http://www.catch22.net/tuts/tips
		// this tests if you're on the non client area hit test
		UINT uHitTest = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);
		if (uHitTest == HTCLIENT)
			return HTCAPTION;
		else
			return uHitTest;
	}

	case WM_CLOSE:
		Minimize();
		return 0;
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

