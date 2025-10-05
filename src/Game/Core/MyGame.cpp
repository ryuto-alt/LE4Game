// MyGame.cpp
#include "MyGame.h"
#include <cassert>

MyGame::MyGame() : engine_(nullptr) {
    // コンストラクタでは特に何もしない
}

MyGame::~MyGame() {
    // デストラクタでは特に何もしない
}

void MyGame::Initialize() {
    try {
        // UnoEngineのインスタンス取得
        engine_ = UnoEngine::GetInstance();

        // エンジンの初期化
        engine_->Initialize();

        // シーンマネージャーの初期化
        engine_->GetSceneManager()->Initialize();

        // 初期シーンへの遷移 - 直接GamePlaySceneに
        engine_->GetSceneManager()->ChangeScene("GamePlay");
    }
    catch (const std::exception&) {
        // エラーは無視
    }
}

void MyGame::Update() {
    // UnoEngineの更新処理を呼び出す
    if (engine_) {
        engine_->Update();

        // 終了リクエストがあれば反映
        if (engine_->IsEndRequested()) {
            endRequest_ = true;
        }
    }
}

void MyGame::Draw() {
    // UnoEngineの描画処理を呼び出す
    if (engine_) {
        engine_->Draw();
    }
}

void MyGame::Finalize() {
    // UnoEngineのFinalizeを呼び出す
    // 注意: シングルトンなのでリソース解放は別途行われる
}