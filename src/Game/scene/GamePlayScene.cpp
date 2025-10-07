#include "GamePlayScene.h"
#include "imgui.h"
#include "UnoEngine.h"
#include "SceneManager.h"
#include "InstancedRenderer.h"

void GamePlayScene::Initialize() {
    UnoEngine* engine = UnoEngine::GetInstance();

    // 必須ポインタのnullチェック
    if (!dxCommon_ || !srvManager_ || !camera_) {
        OutputDebugStringA("GamePlayScene::Initialize - Critical error: Required pointers are null!\n");
        return;
    }

    // ポストプロセス初期化
    postProcess_ = std::make_unique<PostProcess>();
    postProcess_->Initialize(dxCommon_, srvManager_);

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
    skyboxEnabled_ = false;

    // FPSカメラのマウスルックを有効化
    if (fpsCamera_) {
        fpsCamera_->SetMouseLookEnabled(true);
    }

    // 魚眼レンズエフェクトを設定
    if (camera_) {
        camera_->SetFov(1.8f);  // 約103度の広角
    }

    // ポストプロセスで魚眼レンズエフェクトを有効化
    if (postProcess_) {
        // 魚眼レンズの初期強度はメンバ変数で管理
        postProcess_->SetHorrorParams(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);  // ホラーエフェクトはオフ
    }
}


void GamePlayScene::Update() {
    UnoEngine* engine = UnoEngine::GetInstance();

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
    // ポストプロセス用のレンダーターゲットに描画
    if (postProcess_) {
        postProcess_->PreDraw();
    }

    if (skyboxEnabled_ && skybox_) {
        skybox_->Draw(camera_);
    }

    spriteCommon_->CommonDraw();

    if (ground_) {
        ground_->Draw(camera_);
    }

    if (!fpsCamera_ || !fpsCamera_->IsFPSMode()) {
        player_->Draw();
    }

    if (objeObject_) {
        objeObject_->Draw(camera_);
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