#include "GamePlayScene.h"
#include "imgui.h"
#include "UnoEngine.h"
#include "SceneManager.h"
#include "InstancedRenderer.h"
#include <cmath>
#include <filesystem>

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

    // ナビメッシュの初期化
    navMeshManager_ = std::make_unique<NavMeshManager>();
    navMeshManager_->SetLogCallback([this](const std::string& message) {
        AddNavMeshLog(message);
    });

    const std::string navMeshPath = "externals/navimap/stage.navmesh";
    navMeshManager_->Initialize(navMeshPath);

    // NavMeshが読み込まれなかった場合は生成
    if (!navMeshManager_->GetNavMesh() || !navMeshManager_->GetNavMesh()->IsValid()) {
        AddNavMeshLog("No existing NavMesh found, auto-generating...");
        navMeshManager_->GenerateAndSaveNavMesh(sceneObjects_, navMeshPath);
    }

    // 3D空間オーディオリスナーの初期化
    audioListener_ = std::make_unique<SpatialAudioListener>();
    if (player_) {
        audioListener_->SetPosition(player_->GetPosition());
    }

    // EnemyにNavMeshとAudioListenerを設定
    if (enemy_) {
        if (navMeshManager_->GetNavMesh()) {
            enemy_->SetNavMesh(navMeshManager_->GetNavMesh());
            AddNavMeshLog("NavMesh set to Enemy");
        }
        if (player_) {
            enemy_->SetPlayer(player_.get());
        }
        if (audioListener_) {
            enemy_->SetAudioListener(audioListener_.get());
            AddNavMeshLog("Player and AudioListener set to Enemy");
        }
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

#ifdef _DEBUG
    // ImGuiで魚眼レンズ強度と範囲を調整
    ImGui::Begin("Scene Settings");
    ImGui::Text("Camera FOV (degrees): %.2f", sceneData_.camera.fovDegrees);
    ImGui::Text("Camera FOV (radians): %.2f", camera_->GetFovY());
    ImGui::Separator();
    ImGui::SliderFloat("Fisheye Strength", &fisheyeStrength_, 0.0f, 100.0f);
    ImGui::SliderFloat("Fisheye Radius", &fisheyeRadius_, 0.1f, 3.0f);
    ImGui::End();
#endif

    // 魚眼強度と範囲を適用
    if (postProcess_) {
        postProcess_->SetFisheyeStrength(fisheyeStrength_);
        postProcess_->SetFisheyeRadius(fisheyeRadius_);
    }

    HandleInput();
    player_->HandleInput(engine);

    // デルタタイムを取得
    const float deltaTime = engine->GetDeltaTime();

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

    // AudioListenerの位置と向きを更新（プレイヤーの位置とカメラの向き）
    if (audioListener_ && player_ && camera_) {
        audioListener_->SetPosition(player_->GetPosition());
        // カメラの回転からforward vectorを計算
        Vector3 cameraRot = camera_->GetRotate();
        Vector3 forward = {
            std::sin(cameraRot.y),
            0.0f,
            std::cos(cameraRot.y)
        };
        audioListener_->SetOrientation(forward, Vector3{0.0f, 1.0f, 0.0f});
    }

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

    // NavMesh更新
    if (navMeshManager_) {
        navMeshManager_->Update();
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

    // NavMeshの視覚化
    if (navMeshManager_) {
        navMeshManager_->DrawVisualization();
    }

    // ポストプロセスを適用して画面に描画
    if (postProcess_) {
        postProcess_->PostDraw();
    }

#ifdef _DEBUG
    player_->DrawUI();

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

    // NavMeshデバッグウィンドウ
    ImGui::Begin("NavMesh Debug");

    if (navMeshManager_) {
        // NavMeshManagerのImGui描画
        bool showViz = navMeshManager_->IsVisualizationEnabled();
        if (ImGui::Checkbox("Show NavMesh Visualization", &showViz)) {
            navMeshManager_->SetVisualizationEnabled(showViz);
            if (showViz && !navMeshManager_->IsVisualizationEnabled()) {
                // 視覚化を有効にする場合、まだ作成されていなければ作成
                navMeshManager_->CreateVisualization(dxCommon_, camera_);
                navMeshManager_->SetVisualizationEnabled(true);
            }
        }

        NavMeshBuildSettings& settings = navMeshManager_->GetSettings();
        if (ImGui::CollapsingHeader("NavMesh Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Cell Size", &settings.cellSize, 0.05f, 1.0f);
            ImGui::SliderFloat("Cell Height", &settings.cellHeight, 0.05f, 0.5f);
            ImGui::SliderFloat("Agent Height", &settings.agentHeight, 0.5f, 5.0f);
            ImGui::SliderFloat("Agent Radius", &settings.agentRadius, 0.1f, 5.0f);
            ImGui::SliderFloat("Agent Max Climb", &settings.agentMaxClimb, 0.1f, 1.0f);
            ImGui::SliderFloat("Agent Max Slope", &settings.agentMaxSlope, 0.0f, 90.0f);

            ImGui::Separator();
            ImGui::Text("Corner Smoothness Settings");
            ImGui::SliderFloat("Edge Max Error", &settings.edgeMaxError, 0.1f, 3.0f);
            ImGui::SliderFloat("Detail Sample Dist", &settings.detailSampleDist, 1.0f, 10.0f);
        }

        if (ImGui::Button("Generate NavMesh")) {
            ClearNavMeshLogs();
            AddNavMeshLog("=== Manual NavMesh generation triggered ===");
            navMeshManager_->GenerateAndSaveNavMesh(sceneObjects_, "externals/navimap/stage.navmesh");
            if (enemy_ && navMeshManager_->GetNavMesh()) {
                enemy_->SetNavMesh(navMeshManager_->GetNavMesh());
                AddNavMeshLog("NavMesh re-set to Enemy");
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Load NavMesh")) {
            ClearNavMeshLogs();
            const std::string navMeshPath = "externals/navimap/stage.navmesh";
            if (navMeshManager_->LoadNavMesh(navMeshPath)) {
                if (enemy_ && navMeshManager_->GetNavMesh()) {
                    enemy_->SetNavMesh(navMeshManager_->GetNavMesh());
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Logs")) {
            ClearNavMeshLogs();
        }

        ImGui::Separator();

        // NavMesh情報表示
        NavMesh* navMesh = navMeshManager_->GetNavMesh();
        if (navMesh) {
            ImGui::Text("NavMesh Status: %s", navMesh->IsValid() ? "Valid" : "Invalid");

            // シーンオブジェクト数を表示
            ImGui::Text("Scene Objects: %d", static_cast<int>(sceneObjects_.size()));

            if (ImGui::CollapsingHeader("Scene Objects Details")) {
                int idx = 0;
                for (const auto& obj : sceneObjects_) {
                    Model* model = obj->GetModel();
                    if (model) {
                        const ModelData& modelData = model->GetModelData();
                        ImGui::Text("Object %d: %d vertices, %d triangles",
                            idx++,
                            static_cast<int>(modelData.vertices.size()),
                            static_cast<int>(modelData.indices.size()) / 3);
                    }
                }
            }
        } else {
            ImGui::Text("NavMesh: Not Initialized");
        }
    }

    ImGui::Separator();

    // ログ表示
    if (ImGui::CollapsingHeader("NavMesh Logs", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("LogScrolling", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& log : navMeshLogs_) {
            // エラーは赤、成功は緑、それ以外は白
            if (log.find("ERROR") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", log.c_str());
            } else if (log.find("SUCCESS") != std::string::npos) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", log.c_str());
            } else if (log.find("===") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "%s", log.c_str());
            } else {
                ImGui::Text("%s", log.c_str());
            }
        }
        // 自動スクロール
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    ImGui::End();
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

void GamePlayScene::AddNavMeshLog(const std::string& message) {
    navMeshLogs_.push_back(message);
    // 最大1000行まで保持
    if (navMeshLogs_.size() > 1000) {
        navMeshLogs_.erase(navMeshLogs_.begin());
    }
}

void GamePlayScene::ClearNavMeshLogs() {
    navMeshLogs_.clear();
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

    // R キーでナビメッシュ再生成
    if (engine->IsKeyTriggered(DIK_R)) {
        ClearNavMeshLogs();
        AddNavMeshLog("=== Regenerating NavMesh (R key) ===");
        if (navMeshManager_) {
            navMeshManager_->GenerateAndSaveNavMesh(sceneObjects_, "externals/navimap/stage.navmesh");
        }
    }
}

