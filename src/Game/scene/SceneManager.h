#pragma once

#include <memory>
#include <string>
#include "IScene.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "SpriteCommon.h"
#include "SrvManager.h"
#include "Camera.h"

// シーン管理クラス
class SceneManager final {
private:
    // シングルトンインスタンス
    static SceneManager* instance_;

    // コンストラクタ（シングルトン）
    SceneManager() = default;
    // デストラクタ（シングルトン）
    ~SceneManager() = default;
    // コピー禁止
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

public:
    // シングルトンインスタンスの取得
    static SceneManager* GetInstance();

    // 初期化
    void Initialize();

    // 更新
    void Update();

    // 描画
    void Draw();

    // 終了処理
    void Finalize();

    // シーン切り替え
    void ChangeScene(const std::string& sceneName);

    // リソース設定メソッド
    void SetDirectXCommon(DirectXCommon* dxCommon) { dxCommon_ = dxCommon; }
    void SetInput(Input* input) { input_ = input; }
    void SetSpriteCommon(SpriteCommon* spriteCommon) { spriteCommon_ = spriteCommon; }
    void SetSrvManager(SrvManager* srvManager) { srvManager_ = srvManager; }
    void SetWinApp(WinApp* winApp) { winApp_ = winApp; }
    void SetCamera(Camera* camera) { camera_ = camera; }

    // リソース取得メソッド
    WinApp* GetWinApp() const { return winApp_; }

    // 終了リクエスト
    void RequestExit() { shouldExit_ = true; }
    bool ShouldExit() const { return shouldExit_; }

private:
    // 現在のシーン
    std::unique_ptr<IScene> currentScene_;

    // 次のシーン名（シーン切り替え用）
    std::string nextScene_;

    // 共通リソース（各シーンで使用）
    DirectXCommon* dxCommon_ = nullptr;
    Input* input_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Camera* camera_ = nullptr;
    WinApp* winApp_ = nullptr;

    // 終了フラグ
    bool shouldExit_ = false;
};