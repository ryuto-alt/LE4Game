#include "GamePlayScene.h"
#ifdef _DEBUG
#include "imgui.h"
#endif
#include "UnoEngine.h"
#include "SceneManager.h"

void GamePlayScene::Initialize() {
    UnoEngine* engine = UnoEngine::GetInstance();

    // 背景色を黒に設定
    //dxCommon_->SetClearColor(0.1f, 0.25f, 0.5f, 1.0f);
    dxCommon_->SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    lightManager_ = std::make_unique<LightManager>();
    lightManager_->Initialize();

    // FPSカメラの初期化（true: 一人称, false: 三人称）
    fpsCamera_ = std::make_unique<FPSCamera>();
    fpsCamera_->Initialize(true); 

    player_ = std::make_unique<Player>();
    player_->Initialize(camera_, true, false); // コリジョン有効、環境マップ無効
    player_->SetupCamera(engine);

    groundModel_ = engine->CreateAnimatedModel();
    groundModel_->LoadFromFile("Resources/Models/ground", "ground.gltf");

    ground_ = engine->CreateObject3D();

    // 環境マップをHDRファイルで設定
    Object3d::SetEnvTex("Resources/Models/skybox/warm_restaurant_night_2k.hdr");
    ground_->EnableEnv(true);
    ground_->SetModel(static_cast<Model*>(groundModel_.get()));
    ground_->SetCamera(camera_);
    ground_->SetEnableLighting(true);
    ground_->SetPosition({0.0f, -0.1f, 0.0f});

    // Blenderで設定されたスケール情報を使用
    const ModelData& modelData = groundModel_->GetModelData();
    Vector3 blenderScale = modelData.rootTransform.scale;

    // スケールが0の場合はデフォルト値を使用
    if (blenderScale.x == 0.0f && blenderScale.y == 0.0f && blenderScale.z == 0.0f) {
        blenderScale = {40.0f, 40.0f, 40.0f};
    }

    ground_->SetScale(blenderScale);
    

    // 地面の当たり判定を登録
    auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
    if (collisionManager && groundModel_) {
        Collision::AABB groundAABB = Collision::AABBExtractor::ExtractFromAnimatedModel(groundModel_.get());
        collisionManager->RegisterObject(ground_.get(), groundAABB, true, "Ground");
    }

    objeModel_ = engine->CreateAnimatedModel();
    objeModel_->LoadFromFile("Resources/Models/obje", "object.gltf");

    objeObject_ = engine->CreateObject3D();

    // 環境マップを有効化
    objeObject_->EnableEnv(true);
    objeObject_->SetModel(static_cast<Model*>(objeModel_.get()));
    objeObject_->SetCamera(camera_);
    objeObject_->SetPosition({0.0f, 0.0f, 5.0f});
    objeObject_->SetScale({1.0f, 1.0f, 1.0f});
    objeObject_->SetEnableLighting(true);
    objeObject_->SetEnableAnimation(false);
    objeObject_->EnableCollision(true, "Object");

    // SkyboxをDDSファイルで初期化
    skybox_ = engine->CreateSkybox();
    engine->LoadSkybox(skybox_.get(), "Resources/Models/skybox/warm_restaurant_night_2k.hdr");
    skyboxEnabled_ = true;

    // FPSカメラのマウスルックを有効化してマウスカーソルを非表示
    if (fpsCamera_) {
        fpsCamera_->SetMouseLookEnabled(true);
#ifndef _DEBUG
        // Releaseビルドのみマウスカーソルを非表示
        engine->SetMouseCursor(false);
#endif
    }
}


void GamePlayScene::Update() {
    UnoEngine* engine = UnoEngine::GetInstance();

    HandleInput();
    player_->HandleInput(engine);

    // FPSカメラモードかどうかでカメラ更新を切り替え
    if (fpsCamera_ && fpsCamera_->IsFPSMode()) {
        // FPSモード: FPSカメラ専用の更新
        fpsCamera_->UpdateCameraRotation(camera_, engine);
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

    if (ground_) {
        ground_->SetDirectionalLight(dirLight);
        ground_->SetSpotLight(spotLight);
        ground_->Update();
    }

    if (objeObject_) {
        objeObject_->SetDirectionalLight(dirLight);
        objeObject_->SetSpotLight(spotLight);
        objeObject_->Update();
    }

    if (skyboxEnabled_ && skybox_) {
        skybox_->Update();
    }
    player_->Update(engine);

    // ImGui: FPS表示と環境マップ強度調整
#ifdef _DEBUG
    // FPS表示（2秒ごとに更新）
    float deltaTime = engine->GetDeltaTime();
    fpsUpdateTimer_ += deltaTime;
    if (fpsUpdateTimer_ >= 2.0f) {
        displayedFps_ = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
        fpsUpdateTimer_ = 0.0f;
    }

    ImGui::Begin("デバッグ情報");
    ImGui::Text("FPS: %.1f", displayedFps_);
    ImGui::Text("フレーム時間: %.3f ms", deltaTime * 1000.0f);

    // フラスタムカリング情報
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "■ フラスタムカリング統計");
    ImGui::Spacing();

    // オブジェクト統計
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "【オブジェクト】");
    ImGui::Text("  総数: %d", cullingStats_.totalObjects);
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "  描画中: %d", cullingStats_.visibleObjects);
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  カリング済: %d", cullingStats_.culledObjects);

    ImGui::Spacing();

    // メッシュ統計
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "【メッシュ (個別パーツ)】");
    ImGui::Text("  総数: %d", cullingStats_.totalMeshes);
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "  描画中: %d", cullingStats_.visibleMeshes);
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  カリング済: %d", cullingStats_.culledMeshes);

    if (cullingStats_.totalMeshes > 0) {
        float cullPercentage = (float)cullingStats_.culledMeshes / (float)cullingStats_.totalMeshes * 100.0f;
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "  カリング率: %.1f%%", cullPercentage);

        // プログレスバーで視覚化
        float visibleRatio = (float)cullingStats_.visibleMeshes / (float)cullingStats_.totalMeshes;
        ImGui::ProgressBar(visibleRatio, ImVec2(-1, 0), "");
        ImGui::SameLine(0, 5);
        ImGui::Text("描画率");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "※フリーカメラで視点を動かすと");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  画面外のメッシュが非表示になります");

    ImGui::End();

    ImGui::Begin("Environment Map Settings");

    // Ground
    if (ground_ && ground_->IsEnvEnabled()) {
        float groundIntensity = ground_->GetEnvironmentMapIntensity();
        if (ImGui::SliderFloat("Ground Env Intensity", &groundIntensity, 0.0f, 1.0f)) {
            ground_->SetEnvironmentMapIntensity(groundIntensity);
        }
    }

    // Object
    if (objeObject_ && objeObject_->IsEnvEnabled()) {
        float objeIntensity = objeObject_->GetEnvironmentMapIntensity();
        if (ImGui::SliderFloat("Object Env Intensity", &objeIntensity, 0.0f, 1.0f)) {
            objeObject_->SetEnvironmentMapIntensity(objeIntensity);
        }
    }

    ImGui::End();
#endif
}

void GamePlayScene::Draw() {
    // カリング統計をリセット
    cullingStats_.totalObjects = 0;
    cullingStats_.visibleObjects = 0;
    cullingStats_.culledObjects = 0;
    cullingStats_.totalMeshes = 0;
    cullingStats_.visibleMeshes = 0;
    cullingStats_.culledMeshes = 0;

    if (skyboxEnabled_ && skybox_) {
        skybox_->Draw(camera_);
    }

    spriteCommon_->CommonDraw();

    // 地面の描画（メッシュ単位でカリング）
    if (ground_) {
        cullingStats_.totalObjects++;
        int visibleMeshes = 0;
        int culledMeshes = 0;
        ground_->Draw(camera_, &visibleMeshes, &culledMeshes);

        cullingStats_.totalMeshes += (visibleMeshes + culledMeshes);
        cullingStats_.visibleMeshes += visibleMeshes;
        cullingStats_.culledMeshes += culledMeshes;

        if (visibleMeshes > 0) {
            cullingStats_.visibleObjects++;
        } else {
            cullingStats_.culledObjects++;
        }
    }

    // FPSモード以外でプレイヤーを描画（メッシュ単位でカリング）
    if (!fpsCamera_ || !fpsCamera_->IsFPSMode()) {
        cullingStats_.totalObjects++;
        int visibleMeshes = 0;
        int culledMeshes = 0;
        player_->Draw();  // Playerは内部でカリング処理している想定
        cullingStats_.visibleObjects++;
    }

    // オブジェクトの描画（メッシュ単位でカリング）
    if (objeObject_) {
        cullingStats_.totalObjects++;
        int visibleMeshes = 0;
        int culledMeshes = 0;
        objeObject_->Draw(camera_, &visibleMeshes, &culledMeshes);

        cullingStats_.totalMeshes += (visibleMeshes + culledMeshes);
        cullingStats_.visibleMeshes += visibleMeshes;
        cullingStats_.culledMeshes += culledMeshes;

        if (visibleMeshes > 0) {
            cullingStats_.visibleObjects++;
        } else {
            cullingStats_.culledObjects++;
        }
    }

    player_->DrawUI();

#ifdef _DEBUG
    if (lightManager_) {
        lightManager_->DrawImGui();
    }

    // コリジョンデバッグUI
    auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
    if (collisionManager) {
        collisionManager->DrawImGui();
    }
#endif
}

void GamePlayScene::Finalize() {
    if (player_) {
        player_->Finalize();
        player_.reset();
    }
    ground_.reset();
    groundModel_.reset();
    objeObject_.reset();
    objeModel_.reset();
    skybox_.reset();
    lightManager_.reset();
    fpsCamera_.reset();
}

void GamePlayScene::HandleInput() {
    UnoEngine* engine = UnoEngine::GetInstance();

    if (engine->IsKeyTriggered(DIK_F)) {
        lightManager_->ToggleDebugDisplay();
    }

    // Vキーで一人称/三人称切り替え
    if (engine->IsKeyTriggered(DIK_V)) {
        if (fpsCamera_) {
            bool currentMode = fpsCamera_->IsFPSMode();
            fpsCamera_->SetFPSMode(!currentMode);
        }
    }

    // TABキーでマウス視点のON/OFF切り替え（一人称視点時のみ）
    if (engine->IsKeyTriggered(DIK_TAB)) {
        if (fpsCamera_ && fpsCamera_->IsFPSMode()) {
            bool currentMouseLook = fpsCamera_->IsMouseLookEnabled();
            fpsCamera_->SetMouseLookEnabled(!currentMouseLook);

            // マウスカーソルの表示/非表示を切り替え
            if (!currentMouseLook) {
                // マウス視点を有効にする → カーソル非表示
                engine->SetMouseCursor(false);
            } else {
                // マウス視点を無効にする → カーソル表示
                engine->SetMouseCursor(true);
            }
        }
    }

#ifdef _DEBUG
    // フリーカメラモード時のWASD移動
    if (camera_ && camera_->IsFreeCameraMode()) {
        float moveSpeed = 0.1f;

        // Shiftキーで高速移動
        if (engine->IsKeyPressed(DIK_LSHIFT) || engine->IsKeyPressed(DIK_RSHIFT)) {
            moveSpeed *= 3.0f;
        }

        // WASD移動
        if (engine->IsKeyPressed(DIK_W)) {
            camera_->MoveForward(moveSpeed);
        }
        if (engine->IsKeyPressed(DIK_S)) {
            camera_->MoveForward(-moveSpeed);
        }
        if (engine->IsKeyPressed(DIK_A)) {
            camera_->MoveRight(-moveSpeed);
        }
        if (engine->IsKeyPressed(DIK_D)) {
            camera_->MoveRight(moveSpeed);
        }

        // スペース/Ctrlで上下移動
        if (engine->IsKeyPressed(DIK_SPACE)) {
            camera_->MoveUp(moveSpeed);
        }
        if (engine->IsKeyPressed(DIK_LCONTROL) || engine->IsKeyPressed(DIK_RCONTROL)) {
            camera_->MoveUp(-moveSpeed);
        }
    }
#endif
}

void GamePlayScene::UpdateCamera() {
}

void GamePlayScene::DrawUI() {
}