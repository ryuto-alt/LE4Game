#include "GamePlayScene.h"
#include "imgui.h"
#include "UnoEngine.h"
#include "SceneManager.h"
#include "InstancedRenderer.h"
#include <cmath>

void GamePlayScene::Initialize() {
    if (!dxCommon_ || !srvManager_ || !camera_) {
        OutputDebugStringA("GamePlayScene::Initialize - Critical error: Required pointers are null!\n");
        return;
    }
    LoadSceneFromJSON();
    ApplySceneData();
}

void GamePlayScene::LoadSceneFromJSON() {
    sceneData_ = SceneLoader::LoadSceneData("Resources/Scenes/gameplay_scene.json");
}

void GamePlayScene::ApplySceneData() {
    UnoEngine* engine = UnoEngine::GetInstance();

    // Core初期化
    postProcess_ = std::make_unique<PostProcess>();
    postProcess_->Initialize(dxCommon_, srvManager_);
    dxCommon_->SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    lightManager_ = std::make_unique<LightManager>();
    lightManager_->Initialize();

    // 環境設定
    skyboxEnabled_ = sceneData_.environment.skyboxEnabled;
    Object3d::SetEnvTex(sceneData_.environment.environmentMap);

    // カメラ設定（度数をラジアンに変換）
    float fovRadians = sceneData_.camera.fovDegrees * 3.14159265358979323846f / 180.0f;
    camera_->SetFov(fovRadians);

    fpsCamera_ = std::make_unique<FPSCamera>();
    fpsCamera_->Initialize(true);
    fpsCamera_->SetMouseLookEnabled(sceneData_.camera.mouseLookEnabled);

    // ポストプロセス設定
    fisheyeStrength_ = sceneData_.postProcess.fisheyeStrength;
    fisheyeRadius_ = sceneData_.postProcess.fisheyeRadius;
    postProcess_->SetFisheyeStrength(fisheyeStrength_);
    postProcess_->SetFisheyeRadius(fisheyeRadius_);

    const auto& hp = sceneData_.postProcess.horrorParams;
    postProcess_->SetHorrorParams(hp.vignette, hp.aberration, hp.noise, hp.scanlines, hp.distortion);

    // プレイヤー初期化
    player_ = std::make_unique<Player>();
    player_->Initialize(camera_, sceneData_.player.useFPSCamera, sceneData_.player.enableCollision);
    player_->SetupCamera(engine);
    player_->SetPosition(sceneData_.player.position);

    // 敵初期化
    if (!sceneData_.enemies.empty()) {
        enemy_ = std::make_unique<Enemy>();
        enemy_->Initialize(camera_);
        enemy_->SetPosition(sceneData_.enemies[0].position);
        enemy_->SetPlayer(player_.get());
    }

    // オブジェクト初期化
    sceneObjects_ = SceneLoader::CreateObjects(sceneData_, engine);

    // オーディオ初期化
    const auto& bgm = sceneData_.audio.bgm;
    if (!bgm.name.empty() && !bgm.path.empty()) {
        engine->LoadAudio(bgm.name, bgm.path);
        engine->PlayAudio(bgm.name, bgm.loop, bgm.volume);
    }
}


void GamePlayScene::Update() {
    UnoEngine* engine = UnoEngine::GetInstance();

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

    // ImGuiで魚眼レンズ強度と範囲を調整
    ImGui::Begin("Fisheye Settings");
    ImGui::SliderFloat("Fisheye Strength", &fisheyeStrength_, 0.0f, 100.0f);
    ImGui::SliderFloat("Fisheye Radius", &fisheyeRadius_, 0.1f, 3.0f);
    ImGui::End();

    // 魚眼強度と範囲を適用
    if (postProcess_) {
        postProcess_->SetFisheyeStrength(fisheyeStrength_);
        postProcess_->SetFisheyeRadius(fisheyeRadius_);
    }

    HandleInput();
    player_->HandleInput(engine);

    // FPSカメラモードかどうかでカメラ更新を切り替え
    if (fpsCamera_ && fpsCamera_->IsFPSMode()) {
        // FPSモード: FPSカメラ専用の更新
        fpsCamera_->UpdateCameraRotation(camera_, engine);

        // カメラシェイクを更新（プレイヤーの移動状態に基づく）
        fpsCamera_->UpdateCameraShake(player_->IsMoving(), player_->IsRunning(), engine);

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

    if (enemy_) {
        enemy_->SetDirectionalLight(const_cast<DirectionalLight*>(&dirLight));
        enemy_->SetSpotLight(const_cast<SpotLight*>(&spotLight));
        enemy_->Update();
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
    player_->Update(engine);
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