#include "GamePlayScene.h"
#include "imgui.h"
#include "UnoEngine.h"
#include "SceneManager.h"
#include "InstancedRenderer.h"
#include <cmath>
#include <algorithm>

void GamePlayScene::Initialize() {
    if (!dxCommon_ || !srvManager_ || !camera_) {
        OutputDebugStringA("GamePlayScene::Initialize - Critical error: Required pointers are null!\n");
        return;
    }

    SceneConfigurator configurator;
    sceneData_ = configurator.LoadSceneFromJSON("Resources/Scenes/gameplay_scene.json");
    configurator.ApplySceneData(
        sceneData_, dxCommon_, srvManager_, camera_,
        player_, enemy_, sceneObjects_, skybox_, lightManager_,
        fpsCamera_, postProcess_, skyboxEnabled_,
        fisheyeStrength_, fisheyeRadius_
    );

    // フェード用スプライトの初期化
    fadeSprite_ = std::make_unique<Sprite>();
    fadeSprite_->Initialize(spriteCommon_, "Resources/white1x1.png");
    fadeSprite_->SetSize({1280.0f, 720.0f}); // 画面全体を覆うサイズ
    fadeSprite_->SetPosition({0.0f, 0.0f});
    fadeSprite_->setColor({0.0f, 0.0f, 0.0f, fadeAlpha_}); // 黒色、初期は完全不透明

    // 開始演出用の音響効果を読み込み・再生
    UnoEngine* engine = UnoEngine::GetInstance();
    if (engine) {
        // 心臓音（環境音）を読み込んで再生
        engine->LoadAudio("heartbeat", "Resources/Audio/heartbeat.mp3");
        engine->PlayAudio("heartbeat", true, 0.4f); // ループ再生、ボリューム40%

        // 呼吸音を読み込んで再生
        engine->LoadAudio("breathing", "Resources/Audio/breathing.mp3");
        engine->PlayAudio("breathing", true, 0.3f); // ループ再生、ボリューム30%
    }
}


void GamePlayScene::Update() {
    UnoEngine* engine = UnoEngine::GetInstance();

    // デルタタイムを取得
    const float deltaTime = engine->GetDeltaTime();

    // ウィンドウサイズが変わったときにPostProcessのレンダーターゲットをリサイズ
    static uint32_t previousWidth = 0;
    static uint32_t previousHeight = 0;
    uint32_t currentWidth = dxCommon_->GetCurrentWindowWidth();
    uint32_t currentHeight = dxCommon_->GetCurrentWindowHeight();

    if (postProcess_ && (previousWidth != currentWidth || previousHeight != currentHeight)) {
        if (previousWidth != 0 && previousHeight != 0) {  // 初回は除外
            postProcess_->ResizeRenderTarget();
        }
        previousWidth = currentWidth;
        previousHeight = currentHeight;
    }

    // 開始演出の処理
    if (isIntroPlaying_) {
        introTimer_ += deltaTime;

        // Enterキーでスキップ
        if (engine->IsKeyPressed(DIK_RETURN)) {
            isIntroPlaying_ = false;
            fadeAlpha_ = 0.0f;
            vignetteIntensity_ = 0.35f;
            blinkCount_ = maxBlinks_; // 瞬きも終了

            // 環境音を停止
            if (engine) {
                engine->StopAudio("heartbeat");
                engine->StopAudio("breathing");
            }
        } else {
            // 瞬き演出（最初の2秒間）
            if (blinkCount_ < maxBlinks_) {
                blinkTimer_ += deltaTime;

                if (isBlinkClosed_) {
                    // 目を閉じている状態 → 完全に暗い
                    fadeAlpha_ = 1.0f;

                    if (blinkTimer_ >= blinkCloseDuration_) {
                        // 目を開ける
                        isBlinkClosed_ = false;
                        blinkTimer_ = 0.0f;
                        blinkCount_++;
                    }
                } else {
                    // 目を開けている状態 → 徐々に見える
                    float openProgress = blinkTimer_ / blinkOpenDuration_;
                    fadeAlpha_ = 1.0f - (openProgress * 0.5f); // 0.5まで明るくなる

                    if (blinkTimer_ >= blinkOpenDuration_) {
                        // 次の瞬きへ、または瞬き終了
                        if (blinkCount_ < maxBlinks_) {
                            isBlinkClosed_ = true;
                            blinkTimer_ = 0.0f;
                        }
                    }
                }

                // 瞬き中はビネット強め
                vignetteIntensity_ = 1.5f;
            } else {
                // 瞬き終了後、通常のフェードイン
                float timeSinceBlink = introTimer_ - (maxBlinks_ * (blinkOpenDuration_ + blinkCloseDuration_));
                float fadeProgress = (std::min)(timeSinceBlink / (introDuration_ * 0.4f), 1.0f);
                fadeAlpha_ = 0.5f - (fadeProgress * 0.5f); // 0.5 -> 0.0

                // ビネット効果を演出全体の進行度で徐々に弱める（演出終了まで）
                float totalProgress = introTimer_ / introDuration_;
                vignetteIntensity_ = 1.5f - (totalProgress * 1.15f); // 1.5 -> 0.35

                // 目覚めのカメラシェイク（瞬き終了直後）
                if (!introShakePlayed_ && blinkCount_ >= maxBlinks_) {
                    fpsCamera_->TriggerWakeUpShake();
                    introShakePlayed_ = true;
                }
            }

            // 演出終了判定
            if (introTimer_ >= introDuration_) {
                isIntroPlaying_ = false;
                fadeAlpha_ = 0.0f;
                vignetteIntensity_ = 0.35f;

                // 環境音を停止
                if (engine) {
                    engine->StopAudio("heartbeat");
                    engine->StopAudio("breathing");
                }
            }
        }

        // ビネット強度をPostProcessに適用
        if (postProcess_) {
            postProcess_->SetVignetteIntensity(vignetteIntensity_);
        }

        // 演出の進行に応じて環境音のボリュームを調整
        if (engine && introTimer_ > (introDuration_ * 0.7f)) {
            // 演出の最後30%で環境音をフェードアウト
            float fadeOutProgress = (introTimer_ - (introDuration_ * 0.7f)) / (introDuration_ * 0.3f);
            float volume = (std::max)(0.0f, 1.0f - fadeOutProgress);
            engine->SetAudioVolume("heartbeat", volume * 0.4f);
            engine->SetAudioVolume("breathing", volume * 0.3f);
        }
    }

    // ImGuiで魚眼レンズ強度と範囲を調整
    ImGui::Begin("Scene Settings");
    ImGui::Text("Camera FOV (degrees): %.2f", sceneData_.camera.fovDegrees);
    ImGui::Text("Camera FOV (radians): %.2f", camera_->GetFovY());
    ImGui::Separator();
    ImGui::SliderFloat("Fisheye Strength", &fisheyeStrength_, 0.0f, 100.0f);
    ImGui::SliderFloat("Fisheye Radius", &fisheyeRadius_, 0.1f, 3.0f);
    if (ImGui::Button("Reset Intro")) {
        isIntroPlaying_ = true;
        introTimer_ = 0.0f;
        fadeAlpha_ = 1.0f;
        vignetteIntensity_ = 1.5f;
        introShakePlayed_ = false;

        // 瞬き演出もリセット
        blinkCount_ = 0;
        blinkTimer_ = 0.0f;
        isBlinkClosed_ = true;

        // 環境音を再開
        if (engine) {
            engine->StopAudio("heartbeat");
            engine->StopAudio("breathing");
            engine->PlayAudio("heartbeat", true, 0.4f);
            engine->PlayAudio("breathing", true, 0.3f);
        }
    }
    ImGui::End();

    // 魚眼強度と範囲を適用
    if (postProcess_) {
        postProcess_->SetFisheyeStrength(fisheyeStrength_);
        postProcess_->SetFisheyeRadius(fisheyeRadius_);
    }

    // 演出中は入力制限（視点のみ可能）
    if (!isIntroPlaying_) {
        HandleInput();
        player_->HandleInput(engine);
    }

    // フェードスプライトの色を更新
    if (fadeSprite_) {
        fadeSprite_->setColor({0.0f, 0.0f, 0.0f, fadeAlpha_});
        fadeSprite_->Update();
    }

    // FPSカメラモードかどうかでカメラ更新を切り替え
    if (fpsCamera_ && fpsCamera_->IsFPSMode()) {
        // FPSモード: FPSカメラ専用の更新
        fpsCamera_->UpdateCameraRotation(camera_, engine);

        // カメラシェイクを更新（プレイヤーの移動状態に基づく）
        fpsCamera_->UpdateCameraShake(player_->IsMoving(), player_->IsRunning(), deltaTime, engine);

        player_->UpdateFPSCamera(fpsCamera_.get());
        camera_->Update();
    } else {
        // 三人称モード: 通常のカメラシステム
        player_->UpdateCameraSystem(engine);
    }

    lightManager_->Update();

    // スポットライトをプレイヤー視点に追従させる
    if (fpsCamera_) {
        lightManager_->UpdateFlashlight(player_->GetPosition(), fpsCamera_->GetCameraRotation());
    }

    const DirectionalLight& dirLight = lightManager_->GetDirectionalLight();
    const SpotLight& spotLight = lightManager_->GetSpotLight();

    player_->SetDirectionalLight(dirLight);
    player_->SetSpotLight(spotLight);

    // 演出中（瞬き含む）は敵とプレイヤーを更新しない
    if (!isIntroPlaying_) {
        if (enemy_) {
            enemy_->SetDirectionalLight(const_cast<DirectionalLight*>(&dirLight));
            enemy_->SetSpotLight(const_cast<SpotLight*>(&spotLight));
            enemy_->Update();
        }

        player_->Update(engine);
    }

    // 全シーンオブジェクトを更新
    for (auto& obj : sceneObjects_) {
        obj->SetDirectionalLight(dirLight);
        obj->SetSpotLight(spotLight);
        obj->Update();
    }

    if (skyboxEnabled_ && skybox_) {
        skybox_->Update();
    }
}

void GamePlayScene::Draw() {
    // ポストプロセス用のレンダーターゲットに描画
    if (postProcess_) {
        postProcess_->PreDraw();
    }

    if (skyboxEnabled_ && skybox_) {
        skybox_->Draw(camera_);
    }

    spriteCommon_->CommonDraw();

    // 全シーンオブジェクトを描画
    for (auto& obj : sceneObjects_) {
        obj->Draw(camera_);
    }

    if (!fpsCamera_ || !fpsCamera_->IsFPSMode()) {
        player_->Draw();
    }

    if (enemy_) {
        enemy_->Draw();
    }

    // ポストプロセスを適用して画面に描画
    if (postProcess_) {
        postProcess_->PostDraw();
    }

    // フェードオーバーレイを描画（最前面）
    if (fadeSprite_ && fadeAlpha_ > 0.0f) {
        spriteCommon_->CommonDraw();
        fadeSprite_->Draw();
    }

    player_->DrawUI();

#ifdef _DEBUG
    if (lightManager_) {
        lightManager_->DrawImGui();
    }

    if (enemy_) {
        enemy_->DrawUI();
    }

    auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
    if (collisionManager) {
        collisionManager->DrawImGui();
    }
#endif
}

void GamePlayScene::Finalize() {
    // BGMを停止
    UnoEngine* engine = UnoEngine::GetInstance();
    if (engine && !sceneData_.audio.bgm.name.empty()) {
        engine->StopAudio(sceneData_.audio.bgm.name);
    }

    if (player_) {
        player_->Finalize();
        player_.reset();
    }
    if (enemy_) {
        enemy_->Finalize();
        enemy_.reset();
    }
    sceneObjects_.clear();
    skybox_.reset();
    lightManager_.reset();
    fpsCamera_.reset();
    postProcess_.reset();
}

void GamePlayScene::HandleInput() {
    UnoEngine* engine = UnoEngine::GetInstance();

    if (engine->IsKeyTriggered(DIK_F)) {
        lightManager_->ToggleDebugDisplay();
    }

    if (engine->IsKeyTriggered(DIK_V)) {
        if (fpsCamera_) {
            bool currentMode = fpsCamera_->IsFPSMode();
            fpsCamera_->SetFPSMode(!currentMode);
        }
    }

    if (engine->IsKeyTriggered(DIK_TAB)) {
        if (fpsCamera_ && fpsCamera_->IsFPSMode()) {
            fpsCamera_->ToggleMouseLook();
        }
    }
}