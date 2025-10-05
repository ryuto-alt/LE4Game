#include "LogoScene.h"
#include "SceneManager.h"
#include "AudioManager.h"

void LogoScene::Initialize() {
    OutputDebugStringA("LogoScene::Initialize() start\n");

    // 画面クリアカラーを黒に設定
    dxCommon_->SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    camera_->SetTranslate({0.0f, 0.0f, -10.0f});

    // ロゴサウンドの読み込み（再生は1.5秒後）
    AudioManager::GetInstance()->LoadMP3("logoSound", "Resources/Audio/logosound.mp3");
    AudioManager::GetInstance()->SetVolume("logoSound", 0.5f);

    // ロゴスプライトの初期化
    logoSprite_ = std::make_unique<Sprite>();
    OutputDebugStringA("LogoScene: Loading logo.png\n");
    logoSprite_->Initialize(spriteCommon_, "Resources/textures/logo/logo.png");
    OutputDebugStringA("LogoScene: Logo loaded successfully\n");

    // 画面中央に配置
    logoSprite_->SetPosition({ 640.0f, 360.0f });
    logoSprite_->SetAnchorPoint({ 0.5f, 0.5f });

    // 初期状態は完全に透明
    logoSprite_->setColor({ 1.0f, 1.0f, 1.0f, 0.0f });

    // 警告スプライトの初期化
    warningSprite_ = std::make_unique<Sprite>();
    OutputDebugStringA("LogoScene: Loading warning_red01.png\n");
    warningSprite_->Initialize(spriteCommon_, "Resources/textures/warning/warning_red01.png");
    OutputDebugStringA("LogoScene: Warning loaded successfully\n");

    // 画面中央に配置
    warningSprite_->SetPosition({ 640.0f, 360.0f });
    warningSprite_->SetAnchorPoint({ 0.5f, 0.5f });

    // 初期状態は完全に透明
    warningSprite_->setColor({ 1.0f, 1.0f, 1.0f, 0.0f });

    fadeTimer_ = 0.0f;
    displayTimer_ = 0.0f;
    waitTimer_ = 0.0f;
    fadeState_ = FadeState::Wait;
    soundPlayed_ = false;

    OutputDebugStringA("LogoScene::Initialize() completed\n");
}

void LogoScene::Update() {
    camera_->Update();

    float deltaTime = 1.0f / 60.0f;

    switch (fadeState_) {
    case FadeState::Wait:
        waitTimer_ += deltaTime;
        if (waitTimer_ >= kWaitDuration) {
            // 1.5秒経過したのでサウンド再生とロゴフェードイン開始
            if (!soundPlayed_) {
                AudioManager::GetInstance()->Play("logoSound", false);
                soundPlayed_ = true;
            }
            fadeState_ = FadeState::LogoFadeIn;
            fadeTimer_ = 0.0f;
        }
        break;

    case FadeState::LogoFadeIn:
        fadeTimer_ += deltaTime;
        {
            float alpha = fadeTimer_ / kFadeInDuration;
            if (alpha >= 1.0f) {
                alpha = 1.0f;
                fadeState_ = FadeState::LogoDisplay;
                displayTimer_ = 0.0f;
            }
            logoSprite_->setColor({ 1.0f, 1.0f, 1.0f, alpha });
        }
        break;

    case FadeState::LogoDisplay:
        displayTimer_ += deltaTime;
        if (displayTimer_ >= kLogoDisplayDuration) {
            fadeState_ = FadeState::LogoFadeOut;
            fadeTimer_ = 0.0f;
        }
        break;

    case FadeState::LogoFadeOut:
        fadeTimer_ += deltaTime;
        {
            float alpha = 1.0f - (fadeTimer_ / kFadeOutDuration);
            if (alpha <= 0.0f) {
                alpha = 0.0f;
                fadeState_ = FadeState::WaitBeforeWarning;
                waitTimer_ = 0.0f;
            }
            logoSprite_->setColor({ 1.0f, 1.0f, 1.0f, alpha });
        }
        break;

    case FadeState::WaitBeforeWarning:
        waitTimer_ += deltaTime;
        if (waitTimer_ >= kWaitBeforeWarningDuration) {
            fadeState_ = FadeState::WarningFadeIn;
            fadeTimer_ = 0.0f;
        }
        break;

    case FadeState::WarningFadeIn:
        fadeTimer_ += deltaTime;
        {
            float alpha = fadeTimer_ / kFadeInDuration;
            if (alpha >= 1.0f) {
                alpha = 1.0f;
                fadeState_ = FadeState::WarningDisplay;
                displayTimer_ = 0.0f;
            }
            warningSprite_->setColor({ 1.0f, 1.0f, 1.0f, alpha });
        }
        break;

    case FadeState::WarningDisplay:
        displayTimer_ += deltaTime;

        // キーまたはマウスクリックでスキップ
        if (input_->TriggerKey(DIK_SPACE) ||
            input_->TriggerKey(DIK_RETURN) ||
            input_->IsMouseButtonTriggered(0) ||  // 左クリック
            input_->IsMouseButtonTriggered(1)) {  // 右クリック
            fadeState_ = FadeState::WarningFadeOut;
            fadeTimer_ = 0.0f;
        }

        if (displayTimer_ >= kWarningDisplayDuration) {
            fadeState_ = FadeState::WarningFadeOut;
            fadeTimer_ = 0.0f;
        }
        break;

    case FadeState::WarningFadeOut:
        fadeTimer_ += deltaTime;
        {
            float alpha = 1.0f - (fadeTimer_ / kFadeOutDuration);
            if (alpha <= 0.0f) {
                alpha = 0.0f;
                fadeState_ = FadeState::Complete;
            }
            warningSprite_->setColor({ 1.0f, 1.0f, 1.0f, alpha });
        }
        break;

    case FadeState::Complete:
        // タイトルシーンへ遷移
        sceneManager_->ChangeScene("Title");
        break;
    }

    // スペースキーでロゴシーンをスキップ（警告表示前のみ）
    if (fadeState_ != FadeState::WarningDisplay &&
        fadeState_ != FadeState::Complete &&
        (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN))) {
        sceneManager_->ChangeScene("Title");
    }

    logoSprite_->Update();
    warningSprite_->Update();
}

void LogoScene::Draw() {
    // スプライト共通描画設定
    spriteCommon_->CommonDraw();

    // ロゴ描画
    if (fadeState_ == FadeState::LogoFadeIn ||
        fadeState_ == FadeState::LogoDisplay ||
        fadeState_ == FadeState::LogoFadeOut) {
        logoSprite_->Draw();
    }

    // 警告描画
    if (fadeState_ == FadeState::WarningFadeIn ||
        fadeState_ == FadeState::WarningDisplay ||
        fadeState_ == FadeState::WarningFadeOut) {
        warningSprite_->Draw();
    }
}

void LogoScene::Finalize() {
    // ロゴサウンドの停止
    AudioManager::GetInstance()->Stop("logoSound");

    if (logoSprite_) {
        logoSprite_.reset();
    }
    if (warningSprite_) {
        warningSprite_.reset();
    }
}
