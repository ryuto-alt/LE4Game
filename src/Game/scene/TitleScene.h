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
    std::unique_ptr<PostProcess> horrorEffect_;
    std::unique_ptr<PostProcess> whiteNoiseEffect_; // 白黒砂嵐エフェクト
    std::unique_ptr<PostProcess> redStaticEffect_; // 赤い砂嵐エフェクト
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
    const float kInitialNoiseDuration = 0.1f; // 初回砂嵐表示時間
    bool hasPlayedInitialNoise_ = false; // 初回ノイズ音再生済みフラグ

    bool showRandomNoise_ = false;       // ランダム砂嵐表示フラグ
    float randomNoiseTimer_ = 0.0f;      // ランダム砂嵐タイマー
    float nextNoiseTime_ = 0.0f;         // 次の砂嵐までの時間
    const float kRandomNoiseDuration = 0.15f; // ランダム砂嵐表示時間
    const float kMinNoiseInterval = 8.0f;     // 最小間隔
    const float kMaxNoiseInterval = 20.0f;    // 最大間隔
    bool hasPlayedRandomNoise_ = false;  // ランダムノイズ音再生済みフラグ

    // トランジション用
    bool isTransitioning_ = false;       // トランジション中フラグ
    float transitionTimer_ = 0.0f;       // トランジション経過時間
    float transitionTotalTime_ = 0.0f;   // トランジション合計時間（2秒目標）
    float nextTransitionNoiseTime_ = 0.0f; // 次のノイズ表示時間
    bool showTransitionNoise_ = false;   // トランジションノイズ表示中フラグ
    float transitionNoiseTimer_ = 0.0f;  // トランジションノイズタイマー
    float currentNoiseDuration_ = 0.0f;  // 現在のノイズ表示時間
    bool hasPlayedNoiseSound_ = false;   // ノイズ音再生済みフラグ
};