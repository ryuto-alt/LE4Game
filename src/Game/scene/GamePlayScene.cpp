#include "GamePlayScene.h"
#ifdef _DEBUG
#include "imgui.h"
#endif
#include "UnoEngine.h"
#include "SceneManager.h"
#include "InstancedRenderer.h"

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

    // 草モデルのロードと配置（インスタンシング描画で10000個）
    grassModel_ = engine->CreateAnimatedModel();
    grassModel_->LoadFromFile("Resources/Models/glass", "grass01_large.gltf");

    // インスタンシング描画用のレンダラーを作成
    grassRenderer_ = engine->CreateInstancedRenderer(15000);

    // 草を500x500の範囲に10000個配置（100x100グリッド）
    const int gridSizeX = 100;
    const int gridSizeZ = 100;
    const float areaSize = 500.0f; // 配置範囲
    const float spacingX = areaSize / (gridSizeX - 1); // X方向の間隔
    const float spacingZ = areaSize / (gridSizeZ - 1); // Z方向の間隔
    const float startX = -areaSize / 2.0f;
    const float startZ = -areaSize / 2.0f;
    const float groundY = -0.1f; // groundと同じ高さ

    for (int z = 0; z < gridSizeZ; z++) {
        for (int x = 0; x < gridSizeX; x++) {
            GrassInstance instance;
            instance.position = { startX + x * spacingX, groundY, startZ + z * spacingZ };
            instance.scale = { 0.5f, 0.5f, 0.5f };
            grassInstances_.push_back(instance);
        }
    }

    // ドラゴンモデルのロードと配置（インスタンシング描画で5000個）
    dragonModel_ = engine->CreateAnimatedModel();
    dragonModel_->LoadFromFile("Resources/Models/dragon", "dragon.gltf");

    // ドラゴン用インスタンシング描画レンダラーを作成（20000個対応）
    dragonRenderer_ = engine->CreateInstancedRenderer(20000);

    // ドラゴンを広範囲に20000個配置（約141x141グリッド）
    const int dragonGridX = 141;
    const int dragonGridZ = 141;
    const float dragonAreaSize = 5000.0f; // 範囲を広げる
    const float dragonSpacingX = dragonAreaSize / (dragonGridX - 1);
    const float dragonSpacingZ = dragonAreaSize / (dragonGridZ - 1);
    const float dragonStartX = -dragonAreaSize / 2.0f;
    const float dragonStartZ = -dragonAreaSize / 2.0f;
    const float dragonY = 15.0f; // 地面より高く配置

    int dragonCount = 0;
    for (int z = 0; z < dragonGridZ && dragonCount < 20000; z++) {
        for (int x = 0; x < dragonGridX && dragonCount < 20000; x++) {
            DragonInstance instance;
            instance.position = { dragonStartX + x * dragonSpacingX, dragonY, dragonStartZ + z * dragonSpacingZ };
            instance.scale = { 1.0f, 1.0f, 1.0f };
            dragonInstances_.push_back(instance);
            dragonCount++;
        }
    }

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

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "■ インスタンシング描画統計 (LODシステム)");
    ImGui::Spacing();

    // インスタンシング描画オブジェクト統計
    int totalInstances = static_cast<int>(grassInstances_.size()) + static_cast<int>(dragonInstances_.size());
    int renderedGrass = grassRenderer_ ? static_cast<int>(grassRenderer_->GetInstanceCount()) : 0;
    int renderedDragon = dragonRenderer_ ? static_cast<int>(dragonRenderer_->GetInstanceCount()) : 0;
    int totalRendered = renderedGrass + renderedDragon;

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "【総インスタンス数】");
    ImGui::Text("  配置数:   %d", totalInstances);
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "  描画数:   %d", totalRendered);
    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "  削減数:   %d (%.1f%%)",
        totalInstances - totalRendered,
        totalInstances > 0 ? (100.0f * (totalInstances - totalRendered) / totalInstances) : 0.0f);

    ImGui::Spacing();
    ImGui::Separator();

    // 内訳
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "【内訳】");
    ImGui::Text("  草:");
    ImGui::Text("    配置: %d 個", static_cast<int>(grassInstances_.size()));
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "    描画: %d 個", renderedGrass);

    ImGui::Text("  ドラゴン:");
    ImGui::Text("    配置: %d 個", static_cast<int>(dragonInstances_.size()));
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "    描画: %d 個", renderedDragon);

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

    // 草オブジェクトの描画（インスタンシング描画 + LODシステム）
    if (grassRenderer_ && grassModel_ && lightManager_) {
        grassRenderer_->Clear();

        // ライト情報を取得
        const DirectionalLight& dirLight = lightManager_->GetDirectionalLight();
        const SpotLight& spotLight = lightManager_->GetSpotLight();

        // カメラ位置を取得
        Vector3 cameraPos = camera_->GetTranslate();

        // LOD距離を設定（近距離50m、中距離150m、遠距離300m）
        grassRenderer_->SetLODDistances(50.0f, 150.0f, 300.0f);

        // LODシステムを使ってインスタンスを追加
        for (const auto& instance : grassInstances_) {
            Matrix4x4 worldMatrix = {
                instance.scale.x, 0.0f, 0.0f, 0.0f,
                0.0f, instance.scale.y, 0.0f, 0.0f,
                0.0f, 0.0f, instance.scale.z, 0.0f,
                instance.position.x, instance.position.y, instance.position.z, 1.0f
            };
            grassRenderer_->AddInstanceWithLOD(instance.position, worldMatrix, cameraPos, {1.0f, 1.0f, 1.0f, 1.0f});
        }

        // 一括描画
        grassRenderer_->Draw(static_cast<Model*>(grassModel_.get()), camera_, dirLight, spotLight);
    }

    // ドラゴンオブジェクトの描画（インスタンシング描画 + LODシステム）
    if (dragonRenderer_ && dragonModel_ && lightManager_) {
        dragonRenderer_->Clear();

        // ライト情報を取得
        const DirectionalLight& dirLight = lightManager_->GetDirectionalLight();
        const SpotLight& spotLight = lightManager_->GetSpotLight();

        // カメラ位置を取得
        Vector3 cameraPos = camera_->GetTranslate();

        // LOD距離を設定（ドラゴンは大きいので遠くまで見える設定）
        dragonRenderer_->SetLODDistances(100.0f, 300.0f, 600.0f);

        // LODシステムを使ってインスタンスを追加
        for (const auto& instance : dragonInstances_) {
            Matrix4x4 worldMatrix = {
                instance.scale.x, 0.0f, 0.0f, 0.0f,
                0.0f, instance.scale.y, 0.0f, 0.0f,
                0.0f, 0.0f, instance.scale.z, 0.0f,
                instance.position.x, instance.position.y, instance.position.z, 1.0f
            };
            dragonRenderer_->AddInstanceWithLOD(instance.position, worldMatrix, cameraPos, {1.0f, 1.0f, 1.0f, 1.0f});
        }

        // 一括描画
        dragonRenderer_->Draw(static_cast<Model*>(dragonModel_.get()), camera_, dirLight, spotLight);
    }

    player_->DrawUI();

    // カリング統計を表示
    DrawUI();

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
    grassInstances_.clear();
    grassRenderer_.reset();
    grassModel_.reset();
    dragonInstances_.clear();
    dragonRenderer_.reset();
    dragonModel_.reset();
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
    // DrawUI関数は空に（デバッグ情報はUpdate関数内で表示）
}