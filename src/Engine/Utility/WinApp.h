#pragma once
#include <wtypes.h>
#include <Windows.h>
#include <cstdint>
#ifdef _DEBUG
#include "imgui.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif


class WinApp
{
public:

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public: // メンバ関数

	// 初期化
	void Initialize();
	// 更新
	void Update();
	// 終了
	void Finalize();

public:

	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

	HWND GetHwnd() const { return hwnd; }

	HINSTANCE GetHInstance()const { return wc.hInstance; }

	bool ProcessMessage();

	// フルスクリーン切り替え
	void ToggleFullscreen();
	bool IsFullscreen() const { return isFullscreen_; }

private:

	// ウィンドウハンドル
	HWND hwnd = nullptr;
	// ウィンドウクラスの設定
	WNDCLASS wc{};

	// フルスクリーン関連
	bool isFullscreen_ = false;
	RECT windowedRect_ = {};
	LONG windowedStyle_ = 0;
};