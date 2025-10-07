#include "SceneManager.h"
#include "LogoScene.h"
#include "TitleScene.h"
#include "GamePlayScene.h"
#include <cassert>

// 静的メンバ変数の実体化
SceneManager* SceneManager::instance_ = nullptr;

SceneManager* SceneManager::GetInstance() {
    if (!instance_) {
        instance_ = new SceneManager();
    }
    return instance_;
}

void SceneManager::Initialize() {
    // 最初のシーンを設定
#ifdef _DEBUG
    nextScene_ = "GamePlay";  // デバッグ時は直接GamePlayから
#else
    nextScene_ = "Logo";      // リリース時はLogoから
#endif

    // デバッグ出力
    OutputDebugStringA("SceneManager initialized successfully\n");
}

void SceneManager::Update() {
    // SRVヒープを毎フレーム設定（これが重要）
    if (srvManager_) {
        srvManager_->PreDraw();
    }

    // シーン切り替えチェック
    if (!nextScene_.empty()) {
        // デバッグ出力
        OutputDebugStringA(("SceneManager: Changing scene to " + nextScene_ + "\n").c_str());

        // 現在のシーンの終了処理
        if (currentScene_) {
            currentScene_->Finalize();
            currentScene_.reset(); // unique_ptrをクリア
        }

        // 次のシーンを生成
        if (nextScene_ == "Logo") {
            currentScene_ = std::make_unique<LogoScene>();
        } else if (nextScene_ == "Title") {
            currentScene_ = std::make_unique<TitleScene>();
        } else if (nextScene_ == "GamePlay") {
            currentScene_ = std::make_unique<GamePlayScene>();
        }

        // シーンマネージャーのポインタをセット
        currentScene_->SetSceneManager(this);

        // 共通リソースをセット
        currentScene_->SetDirectXCommon(dxCommon_);
        currentScene_->SetInput(input_);
        currentScene_->SetSpriteCommon(spriteCommon_);
        currentScene_->SetSrvManager(srvManager_);
        currentScene_->SetCamera(camera_);

        try {
            // シーンの初期化（例外をキャッチ）
            currentScene_->Initialize();
        }
        catch (const std::exception& e) {
            // エラーを出力
            std::string errorMsg = "Scene initialization error: " + std::string(e.what()) + "\n";
            OutputDebugStringA(errorMsg.c_str());
        }

        // 次のシーン名をクリア
        nextScene_.clear();
    }

    // 現在のシーンの更新
    if (currentScene_) {
        try {
            currentScene_->Update();
        }
        catch (const std::exception&) {
            // エラーは無視
        }
    }
}

void SceneManager::Draw() {
    // SRVヒープを描画前に設定
    if (srvManager_) {
        srvManager_->PreDraw();
    }

    // 現在のシーンの描画
    if (currentScene_) {
        try {
            currentScene_->Draw();
        }
        catch (const std::exception&) {
            // エラーは無視
        }
    }
}

void SceneManager::Finalize() {
    // 現在のシーンの終了処理
    if (currentScene_) {
        try {
            currentScene_->Finalize();
        }
        catch (const std::exception&) {
            // エラーは無視
        }
        currentScene_.reset(); // 明示的にunique_ptrをクリア
    }

    // シングルトンインスタンスの解放
    delete instance_;
    instance_ = nullptr;
}

void SceneManager::ChangeScene(const std::string& sceneName) {
    // 次のシーン名を設定
    nextScene_ = sceneName;

    // デバッグ出力
    OutputDebugStringA(("SceneManager: Scene change requested to " + sceneName + "\n").c_str());
}