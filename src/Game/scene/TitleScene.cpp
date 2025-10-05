#include "TitleScene.h"
#include "../../Engine/Resource/ResourcePreloader.h"
#include "SceneManager.h"
#include <cstdlib>
#include <ctime>
#ifdef _DEBUG
#include "imgui.h"
#endif

void TitleScene::Initialize() {
    camera_->SetTranslate({0.0f, 0.0f, -10.0f});

    // ホラーエフェクトの初期化
    horrorEffect_ = std::make_unique<PostProcess>();
    horrorEffect_->Initialize(dxCommon_, srvManager_);

    // タイトルスプライトの初期化（通常の色で）
    titleBgSprite_ = std::make_unique<Sprite>();
    titleBgSprite_->Initialize(spriteCommon_, "Resources/textures/Title/Title_bg.png");

    titleBg2Sprite_ = std::make_unique<Sprite>();
    titleBg2Sprite_->Initialize(spriteCommon_, "Resources/textures/Title/Title_bg2.png");

    titleTextSprite_ = std::make_unique<Sprite>();
    titleTextSprite_->Initialize(spriteCommon_, "Resources/textures/Title/Title_moji.png");

    hazimeruSprite_ = std::make_unique<Sprite>();
    hazimeruSprite_->Initialize(spriteCommon_, "Resources/textures/Title/hazimeru.png");
    hazimeruOriginalSize_ = hazimeruSprite_->GetSize();
    // Title_bg2の上部黒枠に配置
    hazimeruSprite_->SetPosition({ 640.0f, 500.0f });
    hazimeruSprite_->SetAnchorPoint({ 0.5f, 0.5f }); // 中心を基準に

    owaruSprite_ = std::make_unique<Sprite>();
    owaruSprite_->Initialize(spriteCommon_, "Resources/textures/Title/owaru.png");
    owaruOriginalSize_ = owaruSprite_->GetSize();
    // Title_bg2の下部黒枠に配置
    owaruSprite_->SetPosition({ 640.0f, 590.0f });
    owaruSprite_->SetAnchorPoint({ 0.5f, 0.5f }); // 中心を基準に

    // 砂嵐スプライトの初期化（画面全体を覆う）
    noiseSprite_ = std::make_unique<Sprite>();
    noiseSprite_->Initialize(spriteCommon_, "Resources/textures/Title/noize.png");
    noiseSprite_->SetPosition({ 0.0f, 0.0f });
    noiseSprite_->SetSize({ 1280.0f, 720.0f });
    noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 0.0f }); // 初期は透明

    ResourcePreloader::GetInstance()->PreloadAnimatedModelLightweight("human_walk", "Resources/Models/human", "walk.gltf", dxCommon_);
    ResourcePreloader::GetInstance()->PreloadAnimatedModelLightweight("human_sneak", "Resources/Models/human", "sneakWalk.gltf", dxCommon_);

    // ランダム砂嵐の初期タイミングを設定
    srand(static_cast<unsigned int>(time(nullptr)));
    nextNoiseTime_ = kMinNoiseInterval + static_cast<float>(rand()) / RAND_MAX * (kMaxNoiseInterval - kMinNoiseInterval);

    // タイトルBGMの読み込みと再生
    AudioManager::GetInstance()->LoadMP3("titleBGM", "Resources/Audio/title.mp3");
    AudioManager::GetInstance()->SetVolume("titleBGM", 0.3f);
    AudioManager::GetInstance()->Play("titleBGM", true);
}

void TitleScene::Update() {
    camera_->Update();

    float deltaTime = 1.0f / 60.0f;

    // 初回砂嵐エフェクト
    if (showInitialNoise_) {
        initialNoiseTimer_ += deltaTime;

        // 画像をそのまま表示
        noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        if (initialNoiseTimer_ >= kInitialNoiseDuration) {
            showInitialNoise_ = false;
            noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        }
    }
    // ランダム砂嵐エフェクト
    else {
        randomNoiseTimer_ += deltaTime;

        // 次の砂嵐発生タイミングに到達
        if (!showRandomNoise_ && randomNoiseTimer_ >= nextNoiseTime_) {
            showRandomNoise_ = true;
            randomNoiseTimer_ = 0.0f;
        }

        // 砂嵐表示中
        if (showRandomNoise_) {
            // 画像をそのまま表示
            noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });

            if (randomNoiseTimer_ >= kRandomNoiseDuration) {
                showRandomNoise_ = false;
                noiseSprite_->setColor({ 1.0f, 1.0f, 1.0f, 0.0f });
                randomNoiseTimer_ = 0.0f;
                // 次の砂嵐タイミングをランダムに設定
                nextNoiseTime_ = kMinNoiseInterval + static_cast<float>(rand()) / RAND_MAX * (kMaxNoiseInterval - kMinNoiseInterval);
            }
        }
    }

    // ノイズタイマー更新
    noiseTimer_ += deltaTime;

    // キーボード入力フラグ
    bool keyPressed = false;

    // W/上矢印キーでメニュー選択を上に
    if (input_->TriggerKey(DIK_W) || input_->TriggerKey(DIK_UP)) {
        currentSelection_ = MenuSelection::Start;
        keyPressed = true;
    }
    // S/下矢印キーでメニュー選択を下に
    if (input_->TriggerKey(DIK_S) || input_->TriggerKey(DIK_DOWN)) {
        currentSelection_ = MenuSelection::Exit;
        keyPressed = true;
    }

    // マウスでホバー検知（キーが押されていない時のみ）
    bool hazimeruHovered = false;
    bool owaruHovered = false;
    if (!keyPressed) {
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        ScreenToClient(FindWindowW(L"CG2WindowClass", nullptr), &cursorPos);
        Vector2 mousePos = { static_cast<float>(cursorPos.x), static_cast<float>(cursorPos.y) };

        // アンカーポイントを考慮した判定範囲を計算
        Vector2 hazimeruPos = hazimeruSprite_->GetPosition();
        Vector2 hazimeruMin = { hazimeruPos.x - hazimeruOriginalSize_.x * 0.5f, hazimeruPos.y - hazimeruOriginalSize_.y * 0.5f };
        Vector2 hazimeruMax = { hazimeruPos.x + hazimeruOriginalSize_.x * 0.5f, hazimeruPos.y + hazimeruOriginalSize_.y * 0.5f };

        Vector2 owaruPos = owaruSprite_->GetPosition();
        Vector2 owaruMin = { owaruPos.x - owaruOriginalSize_.x * 0.5f, owaruPos.y - owaruOriginalSize_.y * 0.5f };
        Vector2 owaruMax = { owaruPos.x + owaruOriginalSize_.x * 0.5f, owaruPos.y + owaruOriginalSize_.y * 0.5f };

        if (mousePos.x >= hazimeruMin.x && mousePos.x <= hazimeruMax.x &&
            mousePos.y >= hazimeruMin.y && mousePos.y <= hazimeruMax.y) {
            hazimeruHovered = true;
            currentSelection_ = MenuSelection::Start;
        }
        if (mousePos.x >= owaruMin.x && mousePos.x <= owaruMax.x &&
            mousePos.y >= owaruMin.y && mousePos.y <= owaruMax.y) {
            owaruHovered = true;
            currentSelection_ = MenuSelection::Exit;
        }
    }

    // ノイズエフェクト（グリッチっぽい明滅） - 速度を遅く
    float noiseFlicker = sinf(noiseTimer_ * 10.0f) * 0.5f + 0.5f; // 0.0 ~ 1.0

    // 選択状態に応じた視覚フィードバック
    if (currentSelection_ == MenuSelection::Start || hazimeruHovered) {
        // ノイズエフェクト：明滅のみ
        float brightness = 0.7f + noiseFlicker * 0.3f; // 0.7 ~ 1.0
        hazimeruSprite_->setColor({ brightness, brightness, brightness, 1.0f });

        owaruSprite_->setColor({ 0.5f, 0.5f, 0.5f, 1.0f });
    } else {
        // ノイズエフェクト：明滅のみ
        float brightness = 0.7f + noiseFlicker * 0.3f;
        owaruSprite_->setColor({ brightness, brightness, brightness, 1.0f });

        hazimeruSprite_->setColor({ 0.5f, 0.5f, 0.5f, 1.0f });
    }

    // スプライトの更新
    titleBgSprite_->Update();
    titleBg2Sprite_->Update();
    titleTextSprite_->Update();
    hazimeruSprite_->Update();
    owaruSprite_->Update();
    noiseSprite_->Update();

    // ホラーエフェクトのパラメータ更新
    time_ += 1.0f / 60.0f;
    horrorEffect_->SetHorrorParams(
        time_,
        0.4f,  // ノイズ強度
        0.6f,  // 歪み強度
        0.3f,  // 血エフェクト強度
        0.9f   // ビネット強度（ブラウン管風の丸み）
    );

    // マウスクリックで決定
    DIMOUSESTATE mouseState;
    if (SUCCEEDED(input_->GetMouseState(&mouseState))) {
        if (mouseState.rgbButtons[0] & 0x80) { // 左クリック
            if (hazimeruHovered) {
                sceneManager_->ChangeScene("GamePlay");
            }
            if (owaruHovered) {
                sceneManager_->RequestExit();
            }
        }
    }

    // SPACEまたはENTERで決定
    if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
        if (currentSelection_ == MenuSelection::Start) {
            sceneManager_->ChangeScene("GamePlay");
        } else {
            sceneManager_->RequestExit();
        }
    }

    if (input_->TriggerKey(DIK_ESCAPE)) {
        sceneManager_->RequestExit();
    }
}

void TitleScene::Draw() {
    // ホラーエフェクトのレンダーターゲットに描画開始
    horrorEffect_->PreDraw();

    // スプライト共通描画設定
    spriteCommon_->CommonDraw();

    // 背景だけエフェクトのレンダーターゲットに描画
    titleBgSprite_->Draw();
    titleBg2Sprite_->Draw();

    // 砂嵐エフェクトを最前面に描画
    if (showInitialNoise_ || showRandomNoise_) {
        noiseSprite_->Draw();
    }

    // ホラーエフェクトを適用してバックバッファに描画
    horrorEffect_->PostDraw();

    // エフェクト適用後、タイトル文字をバックバッファに直接描画
    spriteCommon_->CommonDraw();
    titleTextSprite_->Draw();
    hazimeruSprite_->Draw();
    owaruSprite_->Draw();

    
}

bool TitleScene::CheckMouseHover(const Vector2& mousePos, const Vector2& spritePos, const Vector2& spriteSize) {
    return mousePos.x >= spritePos.x && mousePos.x <= spritePos.x + spriteSize.x &&
           mousePos.y >= spritePos.y && mousePos.y <= spritePos.y + spriteSize.y;
}

void TitleScene::Finalize() {
    OutputDebugStringA("TitleScene::Finalize() called\n");

    // タイトルBGMの停止
    AudioManager::GetInstance()->Stop("titleBGM");

    // スプライトの解放
    if (titleBgSprite_) {
        OutputDebugStringA("  Releasing titleBgSprite_\n");
        titleBgSprite_.reset();
    }
    if (titleBg2Sprite_) {
        titleBg2Sprite_.reset();
    }
    if (titleTextSprite_) {
        OutputDebugStringA("  Releasing titleTextSprite_\n");
        titleTextSprite_.reset();
    }
    if (hazimeruSprite_) {
        hazimeruSprite_.reset();
    }
    if (owaruSprite_) {
        owaruSprite_.reset();
    }
    if (noiseSprite_) {
        noiseSprite_.reset();
    }

    // ホラーエフェクトの明示的な解放
    if (horrorEffect_) {
        OutputDebugStringA("  Finalizing horrorEffect_\n");
        horrorEffect_->Finalize();
        horrorEffect_.reset();
    }

    OutputDebugStringA("TitleScene::Finalize() completed\n");
}