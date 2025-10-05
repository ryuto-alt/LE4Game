// MyGame.h
#pragma once

#include "Framework.h"
#include "UnoEngine.h"

class MyGame : public Framework {
public:
    // コンストラクタ・デストラクタ
    MyGame();
    ~MyGame() override;

    // 初期化
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    // UnoEngineのインスタンス
    UnoEngine* engine_ = nullptr;
};