#pragma once
#include "IScene.h"
#include "Sprite.h"
#include <memory>

class LogoScene : public IScene {
public:
    LogoScene() = default;
    ~LogoScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    std::unique_ptr<Sprite> logoSprite_;
    std::unique_ptr<Sprite> warningSprite_;
    float fadeTimer_ = 0.0f;
    float displayTimer_ = 0.0f;
    float waitTimer_ = 0.0f;

    enum class FadeState {
        Wait,              // 待機状態
        LogoFadeIn,        // ロゴフェードイン
        LogoDisplay,       // ロゴ表示
        LogoFadeOut,       // ロゴフェードアウト
        WaitBeforeWarning, // 警告前の待機
        WarningFadeIn,     // 警告フェードイン
        WarningDisplay,    // 警告表示
        WarningFadeOut,    // 警告フェードアウト
        Complete           // 完了
    };
    FadeState fadeState_ = FadeState::Wait;

    // フェード設定
    const float kWaitDuration = 1.5f;              // 初期待機時間
    const float kFadeInDuration = 1.0f;            // フェードイン時間
    const float kLogoDisplayDuration = 2.0f;       // ロゴ表示時間
    const float kFadeOutDuration = 1.0f;           // フェードアウト時間
    const float kWaitBeforeWarningDuration = 1.0f; // 警告前の待機時間（ロゴの後1秒）
    const float kWarningDisplayDuration = 5.0f;    // 警告表示時間

    bool soundPlayed_ = false;  // サウンド再生フラグ
};
