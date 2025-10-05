#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "PostProcess.h"
#include <memory>

class TitleScene : public IScene {
public:
    TitleScene() = default;
    ~TitleScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    std::unique_ptr<Sprite> titleBgSprite_;
    std::unique_ptr<Sprite> titleBg2Sprite_;
    std::unique_ptr<Sprite> titleTextSprite_;
    std::unique_ptr<Sprite> hazimeruSprite_;
    std::unique_ptr<Sprite> owaruSprite_;
    std::unique_ptr<Sprite> noiseSprite_;  // 砂嵐スプライト
    std::unique_ptr<PostProcess> horrorEffect_;
    float time_ = 0.0f;

    // メニュー選択
    enum class MenuSelection {
        Start = 0,  // はじめる
        Exit = 1    // おわる
    };
    MenuSelection currentSelection_ = MenuSelection::Start;
    bool CheckMouseHover(const Vector2& mousePos, const Vector2& spritePos, const Vector2& spriteSize);

    // 選択エフェクト用
    Vector2 hazimeruOriginalSize_;
    Vector2 owaruOriginalSize_;
    float noiseTimer_ = 0.0f;

    // 砂嵐エフェクト用
    bool showInitialNoise_ = true;       // 初回砂嵐表示フラグ
    float initialNoiseTimer_ = 0.0f;     // 初回砂嵐タイマー
    const float kInitialNoiseDuration = 0.3f; // 初回砂嵐表示時間

    bool showRandomNoise_ = false;       // ランダム砂嵐表示フラグ
    float randomNoiseTimer_ = 0.0f;      // ランダム砂嵐タイマー
    float nextNoiseTime_ = 0.0f;         // 次の砂嵐までの時間
    const float kRandomNoiseDuration = 0.15f; // ランダム砂嵐表示時間
    const float kMinNoiseInterval = 8.0f;     // 最小間隔
    const float kMaxNoiseInterval = 20.0f;    // 最大間隔
};