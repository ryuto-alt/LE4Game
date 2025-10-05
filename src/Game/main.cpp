// main.cpp - UnoEngineを使用したサンプル
#include "UnoEngine.h"
#include "D3DResourceCheck.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
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
        engine->GetSceneManager()->Initialize();

        // ゲームループの実行
        engine->Run();

        // エンジンの終了処理
        UnoEngine::DestroyInstance();

        // COM終了処理
        CoUninitialize();
    }
    catch (const std::exception& e) {
        // 例外発生時のエラーメッセージ表示
        MessageBoxA(nullptr, e.what(), "エラーが発生しました", MB_OK | MB_ICONERROR);
        
        // エラー時も終了処理を実行
        UnoEngine::DestroyInstance();
        
        // COM終了処理
        CoUninitialize();
        
        return -1;
    }

    return 0;
}