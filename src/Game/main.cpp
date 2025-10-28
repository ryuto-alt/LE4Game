// main.cpp - UnoEngineを使用したサンプル
#include "UnoEngine.h"
#include "D3DResourceCheck.h"
#include <thread>
#include <chrono>
#include <gdiplus.h>
#include <mmsystem.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

// NVIDIAのOptimusとAMDのPowerXpressに高性能GPUを使うように指示
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Jumpscare表示用のウィンドウプロシージャ
LRESULT CALLBACK JumpscareWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        // 背景消去を防ぐ（ちらつき防止）
        return 1;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Jumpscare表示関数
void ShowJumpscare() {
    // 5秒待機（デスクトップで油断している時間）
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // GDI+初期化
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // GIF画像を読み込み
    Gdiplus::Image* gifImage = new Gdiplus::Image(L"Resources/textures/jumpscare.gif");

    if (gifImage->GetLastStatus() != Gdiplus::Ok) {
        delete gifImage;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return;
    }

    // ウィンドウクラス登録
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = JumpscareWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"JumpscareWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    RegisterClassExW(&wc);

    // フルスクリーンウィンドウ作成
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"JumpscareWindow",
        L"",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );

    if (!hwnd) {
        delete gifImage;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return;
    }

    // ウィンドウ表示
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // 音声再生（noise.mp3をMCIで再生）
    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, currentDir);
    wchar_t audioPath[MAX_PATH];
    swprintf_s(audioPath, L"%s\\Resources\\Audio\\noise.mp3", currentDir);

    wchar_t mciCommand[512];
    swprintf_s(mciCommand, L"open \"%s\" type mpegvideo alias jumpscareAudio", audioPath);
    mciSendStringW(mciCommand, nullptr, 0, nullptr);
    mciSendStringW(L"play jumpscareAudio repeat", nullptr, 0, nullptr);

    // ダブルバッファリング用のバックバッファ作成
    HDC hdcScreen = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    // GDI+ Graphicsオブジェクト（バックバッファ用）
    Gdiplus::Graphics* backBuffer = new Gdiplus::Graphics(hdcMem);

    // 高速化のための設定
    backBuffer->SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    backBuffer->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    backBuffer->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    backBuffer->SetCompositingQuality(Gdiplus::CompositingQualityHighSpeed);

    // GIFアニメーション用のフレーム情報取得
    UINT frameCount = gifImage->GetFrameCount(&Gdiplus::FrameDimensionTime);

    // フレーム遅延時間を取得
    UINT propertySize = gifImage->GetPropertyItemSize(PropertyTagFrameDelay);
    Gdiplus::PropertyItem* propertyItem = nullptr;
    if (propertySize > 0) {
        propertyItem = (Gdiplus::PropertyItem*)malloc(propertySize);
        gifImage->GetPropertyItem(PropertyTagFrameDelay, propertySize, propertyItem);
    }

    // デルタタイムで4秒間表示
    auto startTime = std::chrono::high_resolution_clock::now();
    const float DISPLAY_DURATION = 4.0f; // 4秒間表示
    float elapsedTime = 0.0f;
    UINT currentFrame = 0;
    auto lastFrameTime = startTime;
    float frameTimer = 0.0f;
    bool needsRedraw = true; // 最初は描画が必要

    // フレーム遅延を事前に取得
    float* frameDelays = nullptr;
    if (frameCount > 1 && propertyItem) {
        frameDelays = new float[frameCount];
        LONG* delays = (LONG*)propertyItem->value;
        for (UINT i = 0; i < frameCount; i++) {
            frameDelays[i] = delays[i] * 0.01f; // 10ms単位
            if (frameDelays[i] < 0.01f) frameDelays[i] = 0.033f; // 最低33ms (30fps)
        }
    }

    while (elapsedTime < DISPLAY_DURATION) {
        // デルタタイム計算
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        elapsedTime = std::chrono::duration<float>(currentTime - startTime).count();
        lastFrameTime = currentTime;
        frameTimer += deltaTime;

        // メッセージ処理（ノンブロッキング）
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                if (frameDelays) delete[] frameDelays;
                goto cleanup;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 次のフレームへ進む
        if (frameCount > 1 && frameDelays && frameTimer >= frameDelays[currentFrame]) {
            frameTimer -= frameDelays[currentFrame];
            currentFrame = (currentFrame + 1) % frameCount;

            // GIFのフレームを選択
            GUID pageGuid = Gdiplus::FrameDimensionTime;
            gifImage->SelectActiveFrame(&pageGuid, currentFrame);

            needsRedraw = true; // フレームが変わったので再描画必要
        }

        // フレームが変わった時だけ描画
        if (needsRedraw) {
            // バックバッファに描画（Clearは重いので塗りつぶし）
            backBuffer->DrawImage(gifImage, 0, 0, screenWidth, screenHeight);

            // バックバッファを画面にBlit（一度に転送）
            BitBlt(hdcScreen, 0, 0, screenWidth, screenHeight, hdcMem, 0, 0, SRCCOPY);

            needsRedraw = false;
        }

        // 少し待機（CPU使用率を抑える）
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // フレーム遅延配列のクリーンアップ
    if (frameDelays) {
        delete[] frameDelays;
    }

cleanup:
    // 音声停止（MCI）
    mciSendStringW(L"stop jumpscareAudio", nullptr, 0, nullptr);
    mciSendStringW(L"close jumpscareAudio", nullptr, 0, nullptr);

    // クリーンアップ
    delete backBuffer;
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcScreen);

    if (propertyItem) {
        free(propertyItem);
    }

    DestroyWindow(hwnd);
    delete gifImage;
    Gdiplus::GdiplusShutdown(gdiplusToken);

    // コンティニュー確認ダイアログ
    int result = MessageBoxW(
        nullptr,
        L"コンティニューしますか？",
        L"ゲームオーバー",
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST
    );

    if (result == IDYES) {
        // 「はい」が押された場合、ゲームを再起動
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};

        // 通常モードで起動（引数なし）
        if (CreateProcessA(
            exePath,
            nullptr,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi
        )) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    // コマンドライン引数をチェック
    if (lpCmdLine && strstr(lpCmdLine, "--jumpscare")) {
        // Jumpscareモード: ゲームを起動せずにjumpscareを表示
        ShowJumpscare();
        return 0;
    }

    // 通常のゲームモード
    // リソースリーク検出用
    D3DResourceLeakChecker leakCheck;

    try {
        // COM初期化
        CoInitializeEx(0, COINIT_MULTITHREADED);

        // エンジンのインスタンスを取得
        UnoEngine* engine = UnoEngine::GetInstance();

        // エンジンの初期化
        engine->Initialize();

        // シーンマネージャーの初期化（内部でLogoシーンに設定される）
        engine->GetScnMgr()->Initialize();

        // ゲームループの実行
        engine->Run();

        // エンジンの終了処理
        UnoEngine::DestroyInst();

        // COM終了処理
        CoUninitialize();
    }
    catch (const std::exception& e) {
        // 例外発生時のエラーメッセージ表示
        MessageBoxA(nullptr, e.what(), "エラーが発生しました", MB_OK | MB_ICONERROR);

        // エラー時も終了処理を実行
        UnoEngine::DestroyInst();

        // COM終了処理
        CoUninitialize();

        return -1;
    }

    return 0;
}