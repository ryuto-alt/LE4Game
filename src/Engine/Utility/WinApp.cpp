#include "WinApp.h"

LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
#ifdef _DEBUG
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WinApp::Initialize()
{
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
#pragma region ウィンドウクラスの登録


	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClass(&wc);
#pragma endregion

#pragma region ウィンドウサイズを決める

	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
#pragma endregion

#pragma region ウィンドウ生成と表示
	hwnd = CreateWindow(
		wc.lpszClassName,
		L"UnoEngineGame",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);
#pragma endregion

	ShowWindow(hwnd, SW_SHOW);


}

void WinApp::Update()
{
}

void WinApp::Finalize()
{
	CloseWindow(hwnd);
	CoUninitialize();
}

bool WinApp::ProcessMessage()
{
	MSG msg{};
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	if (msg.message==WM_QUIT) {
		return true;
	}

	return false;
}

void WinApp::ToggleFullscreen()
{
	if (!isFullscreen_) {
		// ウィンドウモード → フルスクリーン（ボーダーレスウィンドウ方式）
		// 現在のウィンドウスタイルと位置を保存
		windowedStyle_ = GetWindowLong(hwnd, GWL_STYLE);
		GetWindowRect(hwnd, &windowedRect_);

		// ウィンドウスタイルを変更（枠なし、最大化ボタンなし）
		SetWindowLong(hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);

		// モニター情報を取得
		HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
		GetMonitorInfo(hMonitor, &monitorInfo);

		// フルスクリーンサイズを保存
		fullscreenWidth_ = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
		fullscreenHeight_ = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

		// フルスクリーンサイズに変更（モニターの作業領域全体）
		SetWindowPos(hwnd, HWND_TOPMOST,
			monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.top,
			fullscreenWidth_,
			fullscreenHeight_,
			SWP_FRAMECHANGED | SWP_SHOWWINDOW);

		isFullscreen_ = true;
	}
	else {
		// フルスクリーン → ウィンドウモード
		// 元のウィンドウスタイルに戻す
		SetWindowLong(hwnd, GWL_STYLE, windowedStyle_);

		// 元のサイズと位置に戻す（HWND_NOTOPMOSTで最前面から外す）
		SetWindowPos(hwnd, HWND_NOTOPMOST,
			windowedRect_.left,
			windowedRect_.top,
			windowedRect_.right - windowedRect_.left,
			windowedRect_.bottom - windowedRect_.top,
			SWP_FRAMECHANGED | SWP_SHOWWINDOW);

		fullscreenWidth_ = 0;
		fullscreenHeight_ = 0;
		isFullscreen_ = false;
	}
}

uint32_t WinApp::GetCurrentWindowWidth() const
{
	if (isFullscreen_ && fullscreenWidth_ > 0) {
		return fullscreenWidth_;
	}
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	return clientRect.right - clientRect.left;
}

uint32_t WinApp::GetCurrentWindowHeight() const
{
	if (isFullscreen_ && fullscreenHeight_ > 0) {
		return fullscreenHeight_;
	}
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	return clientRect.bottom - clientRect.top;
}
