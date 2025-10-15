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

    // 白黒砂嵐エフェクトの初期化
    whiteNoiseEffect_ = std::make_unique<PostProcess>();
    whiteNoiseEffect_->Initialize(dxCommon_, srvManager_);

    // 赤い砂嵐エフェクトの初期化
    redStaticEffect_ = std::make_unique<PostProcess>();
    redStaticEffect_->Initialize(dxCommon_, srvManager_);

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
        // 初回砂嵐の音を再生（1回だけ）
        if (!hasPlayedInitialNoise_) {
            AudioManager::GetInstance()->LoadMP3("initialNoise", "Resources/Audio/noize.mp3");
            AudioManager::GetInstance()->SetVolume("initialNoise", 0.3f);
            AudioManager::GetInstance()->Play("initialNoise", false); // ループしない
            hasPlayedInitialNoise_ = true;
        }

        initialNoiseTimer_ += deltaTime;

        if (initialNoiseTimer_ >= kInitialNoiseDuration) {
            showInitialNoise_ = false;
            AudioManager::GetInstance()->Stop("initialNoise");
        }
    }
    // ランダム砂嵐エフェクト
    else {
        randomNoiseTimer_ += deltaTime;

        // 次の砂嵐発生タイミングに到達
        if (!showRandomNoise_ && randomNoiseTimer_ >= nextNoiseTime_) {
            showRandomNoise_ = true;
            randomNoiseTimer_ = 0.0f;
            hasPlayedRandomNoise_ = false; // フラグをリセット
        }

        // 砂嵐表示中
        if (showRandomNoise_) {
            // ランダム砂嵐の音を再生（1回だけ）
            if (!hasPlayedRandomNoise_) {
                AudioManager::GetInstance()->LoadMP3("randomNoise", "Resources/Audio/noize.mp3");
                AudioManager::GetInstance()->SetVolume("randomNoise", 0.3f);
                AudioManager::GetInstance()->Play("randomNoise", false); // ループしない
                hasPlayedRandomNoise_ = true;
            }

            if (randomNoiseTimer_ >= kRandomNoiseDuration) {
                showRandomNoise_ = false;
                AudioManager::GetInstance()->Stop("randomNoise");
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
                // トランジション開始
                isTransitioning_ = true;
                transitionTimer_ = 0.0f;
                transitionTotalTime_ = 0.0f;
                nextTransitionNoiseTime_ = 0.0f; // すぐに最初のノイズを表示
                showTransitionNoise_ = false;
                hasPlayedNoiseSound_ = false; // フラグをリセット
                isLastNoise_ = false; // 最後の砂嵐フラグをリセット
            }
            if (owaruHovered) {
                sceneManager_->RequestExit();
            }
        }
    }

    // トランジション中の処理
    if (isTransitioning_) {
        transitionTimer_ += deltaTime;

        // トランジションノイズの更新
        if (showTransitionNoise_) {
            transitionNoiseTimer_ += deltaTime;
            // ノイズ表示中（シェーダーで描画）

            if (transitionNoiseTimer_ >= currentNoiseDuration_) {
                // ノイズ終了
                showTransitionNoise_ = false;
                transitionNoiseTimer_ = 0.0f;
                transitionTotalTime_ += currentNoiseDuration_;

                // ノイズ音停止
                if (hasPlayedNoiseSound_) {
                    AudioManager::GetInstance()->Stop("transitionNoise");
                    hasPlayedNoiseSound_ = false;
                }

                // 最後の砂嵐が終わった場合は即座に暗転モードに
                // （次のノイズタイミング設定をスキップ）

                // 最後の砂嵐でない場合のみ、次のノイズタイミングをランダムに設定
                if (!isLastNoise_) {
                    float minInterval = 0.05f;
                    float maxInterval = 0.2f;
                    nextTransitionNoiseTime_ = transitionTimer_ + minInterval +
                        static_cast<float>(rand()) / RAND_MAX * (maxInterval - minInterval);
                }
            }
        } else {
            // ノイズ非表示

            // 0.7秒経過したら最後の砂嵐を開始
            if (transitionTotalTime_ >= 0.7f && !isLastNoise_) {
                showTransitionNoise_ = true;
                transitionNoiseTimer_ = 0.0f;
                isLastNoise_ = true;

                // 最後の砂嵐は長めに（1.0秒）
                currentNoiseDuration_ = 1.0f;

                // ノイズ音再生
                if (!hasPlayedNoiseSound_) {
                    AudioManager::GetInstance()->LoadMP3("transitionNoise", "Resources/Audio/noize.mp3");
                    AudioManager::GetInstance()->SetVolume("transitionNoise", 0.5f);
                    AudioManager::GetInstance()->Play("transitionNoise", false); // ループしない
                    hasPlayedNoiseSound_ = true;
                }
            }
            // 次のノイズ表示タイミングチェック（最後の砂嵐前まで）
            else if (transitionTimer_ >= nextTransitionNoiseTime_ && transitionTotalTime_ < 0.7f) {
                showTransitionNoise_ = true;
                transitionNoiseTimer_ = 0.0f;

                // ノイズ表示時間をランダムに設定（0.2秒～0.3秒）
                float minDuration = 0.2f;
                float maxDuration = 0.3f;
                currentNoiseDuration_ = minDuration +
                    static_cast<float>(rand()) / RAND_MAX * (maxDuration - minDuration);

                // ノイズ音再生
                if (!hasPlayedNoiseSound_) {
                    AudioManager::GetInstance()->LoadMP3("transitionNoise", "Resources/Audio/noize.mp3");
                    AudioManager::GetInstance()->SetVolume("transitionNoise", 0.5f);
                    AudioManager::GetInstance()->Play("transitionNoise", false); // ループしない
                    hasPlayedNoiseSound_ = true;
                }
            }
        }

        // 最後の砂嵐が終わって0.3秒経過したらシーン遷移（黒画面を確実に表示）
        if (isLastNoise_ && !showTransitionNoise_ && transitionTimer_ >= transitionTotalTime_ + 0.3f) {
            sceneManager_->ChangeScene("GamePlay");
        }

        return; // トランジション中は他の更新を停止
    }

    // SPACEまたはENTERで決定
    if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
        if (currentSelection_ == MenuSelection::Start) {
            // トランジション開始
            isTransitioning_ = true;
            transitionTimer_ = 0.0f;
            transitionTotalTime_ = 0.0f;
            nextTransitionNoiseTime_ = 0.0f; // すぐに最初のノイズを表示
            showTransitionNoise_ = false;
            hasPlayedNoiseSound_ = false; // フラグをリセット
            isLastNoise_ = false; // 最後の砂嵐フラグをリセット
        } else {
            sceneManager_->RequestExit();
        }
    }

    if (input_->TriggerKey(DIK_ESCAPE)) {
        sceneManager_->RequestExit();
    }
}

void TitleScene::Draw() {
    // トランジション中でない場合のみ背景を描画
    if (!isTransitioning_) {
        // ホラーエフェクトのレンダーターゲットに描画開始
        horrorEffect_->PreDraw();

        // スプライト共通描画設定
        spriteCommon_->CommonDraw();

        // 背景だけエフェクトのレンダーターゲットに描画
        titleBgSprite_->Draw();
        titleBg2Sprite_->Draw();

        // ホラーエフェクトを適用してバックバッファに描画
        horrorEffect_->PostDraw();
    }

    // 砂嵐エフェクトをシェーダーで描画（通常時のみ）
    if (!isTransitioning_ && (showInitialNoise_ || showRandomNoise_)) {
        // 白黒砂嵐シェーダーに切り替え
        whiteNoiseEffect_->UseWhiteNoiseShader();
        whiteNoiseEffect_->SetWhiteNoiseParams(time_, 1.0f); // 強度MAX

        // 白黒砂嵐をフルスクリーンで描画
        whiteNoiseEffect_->PreDraw();

        // スプライト共通描画設定
        spriteCommon_->CommonDraw();

        // 何も描画しない（透明な砂嵐のみ）

        // 白黒砂嵐シェーダーを適用
        whiteNoiseEffect_->PostDraw();

        // 通常シェーダーに戻す
        whiteNoiseEffect_->UseHorrorShader();
    }

    // トランジション中の赤い砂嵐エフェクト
    if (isTransitioning_ && showTransitionNoise_) {
        // 赤い砂嵐シェーダーに切り替え
        redStaticEffect_->UseRedStaticShader();
        redStaticEffect_->SetRedStaticParams(time_, 1.0f); // 強度MAX

        // 赤い砂嵐をフルスクリーンで描画
        redStaticEffect_->PreDraw();

        // スプライト共通描画設定
        spriteCommon_->CommonDraw();

        // 何も描画しない（黒背景に砂嵐のみ）

        // 赤い砂嵐シェーダーを適用
        redStaticEffect_->PostDraw();

        // 通常シェーダーに戻す
        redStaticEffect_->UseHorrorShader();
    }
    // トランジション中で砂嵐が表示されていない時
    if (isTransitioning_ && !showTransitionNoise_) {
        // 最後の砂嵐後は完全な黒画面
        if (isLastNoise_) {
            // 何も描画しない = 黒画面（クリアカラーが黒なので）
        }
        // 最後の砂嵐前はタイトル文字を表示
        else {
            spriteCommon_->CommonDraw();
            titleTextSprite_->Draw();
            hazimeruSprite_->Draw();
            owaruSprite_->Draw();
        }
    }
    // 通常時のタイトル文字表示
    else if (!isTransitioning_) {
        spriteCommon_->CommonDraw();
        titleTextSprite_->Draw();
        hazimeruSprite_->Draw();
        owaruSprite_->Draw();
    }
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

    // 白黒砂嵐エフェクトの明示的な解放
    if (whiteNoiseEffect_) {
        OutputDebugStringA("  Finalizing whiteNoiseEffect_\n");
        whiteNoiseEffect_->Finalize();
        whiteNoiseEffect_.reset();
    }

    // ホラーエフェクトの明示的な解放
    if (horrorEffect_) {
        OutputDebugStringA("  Finalizing horrorEffect_\n");
        horrorEffect_->Finalize();
        horrorEffect_.reset();
    }

    // 赤い砂嵐エフェクトの明示的な解放
    if (redStaticEffect_) {
        OutputDebugStringA("  Finalizing redStaticEffect_\n");
        redStaticEffect_->Finalize();
        redStaticEffect_.reset();
    }

    OutputDebugStringA("TitleScene::Finalize() completed\n");
}