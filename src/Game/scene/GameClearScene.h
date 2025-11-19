#pragma once
#include "IScene.h"
#include "Sprite.h"
#include <memory>
#include <vector>
#include <string>

class GameClearScene : public IScene {
public:
    GameClearScene() = default;
    ~GameClearScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    float time_ = 0.0f;

    // 黒背景用スプライト
    std::unique_ptr<Sprite> blackBgSprite_;

    // タイトル画像用スプライト
    std::unique_ptr<Sprite> titleImageSprite_;

    // ロゴ画像用スプライト
    std::unique_ptr<Sprite> logoImageSprite_;

    // エンドロール用
    struct CreditLine {
        std::string text;
        bool isTitle;  // タイトル行かどうか
        bool isImage;  // 画像として表示するかどうか
    };
    std::vector<CreditLine> credits_;
    float scrollOffset_ = 0.0f;
    const float scrollSpeed_ = 30.0f;  // ピクセル/秒
    bool creditsFinished_ = false;

    void InitializeCredits();
    void DrawCredits();
};
