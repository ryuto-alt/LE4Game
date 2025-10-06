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
    player_->Initialize(camera_, true, true); // コリジョン有効、環境マップ有効
    player_->SetupCamera(engine);

    groundModel_ = engine->CreateAnimatedModel();
    groundModel_->LoadFromFile("Resources/Models/ground", "ground.gltf");

    ground_ = engine->CreateObject3D();

    // 環境マップを先に設定（SetModelの前に）
    Object3d::SetEnvTex("Resources/Models/skybox/rostock_laage_airport_4k.dds");
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

    // 環境マップを先に設定（SetModelの前に）
    objeObject_->EnableEnv(true);
    objeObject_->SetModel(static_cast<Model*>(objeModel_.get()));
    objeObject_->SetCamera(camera_);
    objeObject_->SetPosition({0.0f, 0.0f, 5.0f});
    objeObject_->SetScale({1.0f, 1.0f, 1.0f});
    objeObject_->SetEnableLighting(true);
    objeObject_->SetEnableAnimation(false);
    objeObject_->EnableCollision(true, "Object");

    // Skyboxの初期化
    skybox_ = engine->CreateSkybox();
    engine->LoadSkybox(skybox_.get(), "Resources/Models/skybox/rostock_laage_airport_4k.dds");
    skyboxEnabled_ = true;

    // FPSカメラのマウスルックを有効化してマウスカーソルを非表示
    if (fpsCamera_) {
        fpsCamera_->SetMouseLookEnabled(true);
        engine->SetMouseCursor(false);
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
}

void GamePlayScene::Draw() {
    if (skyboxEnabled_ && skybox_) {
        skybox_->Draw(camera_);
    }

    spriteCommon_->CommonDraw();

    if (ground_) {
        ground_->Draw();
    }

    // FPSモード以外でプレイヤーを描画
    if (!fpsCamera_ || !fpsCamera_->IsFPSMode()) {
        player_->Draw();
    }

    if (objeObject_) {
        objeObject_->Draw();
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
}

void GamePlayScene::UpdateCamera() {
}

void GamePlayScene::DrawUI() {
}