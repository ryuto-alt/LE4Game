#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "PostProcess.h"
#include <memory>

class GameOverScene : public IScene {
public:
    GameOverScene() = default;
    ~GameOverScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    std::unique_ptr<Sprite> gameOverTextSprite_;
    std::unique_ptr<Sprite> retrySprite_;
    std::unique_ptr<Sprite> titleSprite_;
    std::unique_ptr<PostProcess> horrorEffect_;
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
