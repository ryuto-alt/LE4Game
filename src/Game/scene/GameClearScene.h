#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "PostProcess.h"
#include <memory>

class GameClearScene : public IScene {
public:
    GameClearScene() = default;
    ~GameClearScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    std::unique_ptr<Sprite> gameClearTextSprite_;
    std::unique_ptr<Sprite> retrySprite_;
    std::unique_ptr<Sprite> titleSprite_;
    float time_ = 0.0f;

    // メニュー選択
    enum class MenuSelection {
        Retry = 0,  // リトライ
        Title = 1   // タイトルへ
    };
    MenuSelection currentSelection_ = MenuSelection::Retry;
    bool CheckMouseHover(const Vector2& mousePos, const Vector2& spritePos, const Vector2& spriteSize);

    // 選択エフェクト用
    Vector2 retryOriginalSize_;
    Vector2 titleOriginalSize_;
};
