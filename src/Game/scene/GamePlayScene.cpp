#include "GamePlayScene.h"
#include "imgui.h"
#include "UnoEngine.h"
#include "SceneManager.h"
#include "InstancedRenderer.h"
#include "NavMesh/NavMeshSystem.h"
#include "NavMesh/EnemyAI.h"
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

    // ナビメッシュの初期化（UnoEngine経由）
    UnoEngine* engine = UnoEngine::GetInstance();
    NavMeshManager* navMeshManager = engine->GetNavMgr();

    if (navMeshManager) {
        navMeshManager->SetLogCallback([this](const std::string& message) {
            AddNavMeshLog(message);
        });
    }

    const std::string navMeshPath = "externals/navimap/stage.navmesh";
    engine->InitNav(navMeshPath);

    // NavMeshが読み込まれなかった場合は生成
    navMeshManager = engine->GetNavMgr();
    if (!navMeshManager->GetNavMesh() || !navMeshManager->GetNavMesh()->IsValid()) {
        AddNavMeshLog("No existing NavMesh found, auto-generating...");
        engine->GenNav(sceneObjects_, navMeshPath);
    }

    // 3D空間オーディオリスナーの初期化
    audioListener_ = std::make_unique<SpatialAudioListener>();
    if (player_) {
        audioListener_->SetPosition(player_->GetPosition());
    }

    // EnemyにNavMeshとAudioListenerを設定
    if (enemy_) {
        navMeshManager = engine->GetNavMgr();
        if (navMeshManager && navMeshManager->GetNavMesh()) {
            enemy_->SetNavMesh(navMeshManager->GetNavMesh());
            AddNavMeshLog("NavMesh set to Enemy");

            // 新しいAIシステム用にNavMeshSystemを作成して設定
            // 注意: 今は旧NavMeshシステムを使用（useNewAI_ = false）
            // NavMeshSystemのロードに失敗するため、一時的に無効化
            /*
            navMeshSystem_ = std::make_unique<NavMeshSystem>();
            if (navMeshSystem_->LoadNavMeshFromFile("externals/navimap/stage.navmesh")) {
                enemy_->SetNavMeshSystem(navMeshSystem_.get());
                AddNavMeshLog("New AI System initialized with NavMeshSystem");
            } else {
                AddNavMeshLog("Warning: Failed to load NavMeshSystem for new AI");
            }
            */
            // 旧システムを使用
            enemy_->useNewAI_ = false;
            AddNavMeshLog("Using old AI system (NavMeshSystem load failed)");
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
    const float deltaTime = engine->GetDelta();

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

    // NavMesh更新（UnoEngine経由）
    engine->UpdateNavMesh();
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

    // NavMeshの視覚化（UnoEngine経由）
    UnoEngine::GetInstance()->DrawNavVis();

    // NavMeshデバッグプレビュー描画
    {
        auto* navMeshManager = UnoEngine::GetInstance()->GetNavMgr();
        if (navMeshManager) {
            navMeshManager->DrawDebugPreview();
        }
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

    auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
    if (collisionManager) {
        collisionManager->DrawImGui();
    }

    // NavMeshデバッグウィンドウ
    ImGui::Begin("NavMesh Debug");

    UnoEngine* engine = UnoEngine::GetInstance();
    NavMeshManager* navMeshManager = engine->GetNavMgr();

    if (navMeshManager) {
        // NavMeshManagerのImGui描画
        bool showViz = engine->IsNavVis();
        if (ImGui::Checkbox("Show NavMesh Visualization", &showViz)) {
            if (showViz) {
                // 視覚化を有効にする場合、視覚化オブジェクトを作成
                engine->CreateNavVis();
                engine->SetNavVis(true);
                AddNavMeshLog("NavMesh visualization enabled");
            } else {
                // 視覚化を無効にする
                engine->SetNavVis(false);
                AddNavMeshLog("NavMesh visualization disabled");
            }
        }

        // Enemy視界の可視化
        if (enemy_) {
            bool showEnemyVision = enemy_->debugDrawVision_;
            if (ImGui::Checkbox("Show Enemy Vision", &showEnemyVision)) {
                enemy_->debugDrawVision_ = showEnemyVision;
            }
        }

        if (ImGui::CollapsingHeader("About Recast Navigation")) {
            ImGui::TextWrapped("This project uses Recast Navigation, the industry-standard NavMesh library.");
            ImGui::TextWrapped("Used in: Unreal Engine, Unity, many AAA games");
            ImGui::Separator();
            ImGui::Text("Precision depends on settings:");
            ImGui::BulletText("Lower Cell Size = Higher precision (0.1-0.2 recommended)");
            ImGui::BulletText("Smaller Agent Radius = Closer to walls (0.3-0.6)");
            ImGui::BulletText("Lower Edge Max Error = Smoother paths (0.5-1.0)");
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "This is professional-grade pathfinding!");
            ImGui::TextWrapped("If precision seems low, try the High Precision Preset below.");
        }

        NavMeshBuildSettings& settings = engine->GetNavSet();

        // リアルタイムプレビュー機能
        static bool showPreview = false;
        static NavMeshBuildSettings lastSettings = settings;
        bool settingsChanged = false;

        // プレビュー範囲の設定
        static float previewRadius = 30.0f;  // Enemyの周りに表示する範囲
        static bool useEnemyCenter = true;   // Enemyを中心にするか

        if (ImGui::Checkbox("Show Grid Preview (Real-time)", &showPreview)) {
            navMeshManager->SetDebugPreviewEnabled(showPreview);
        }

        if (showPreview) {
            ImGui::SameLine();
            ImGui::Checkbox("Center on Enemy", &useEnemyCenter);

            // Agent設定に基づいた表示範囲
            static bool useAgentSettings = true;
            static bool showBoundingBox = true;
            static bool showGrid = true;
            ImGui::Checkbox("Use Agent Settings for Preview", &useAgentSettings);
            ImGui::Checkbox("Show Grid (Cyan)", &showGrid);
            ImGui::Checkbox("Show Bounding Box (Yellow)", &showBoundingBox);

            if (!useAgentSettings) {
                ImGui::SliderFloat("Preview Radius", &previewRadius, 10.0f, 100.0f);
            }

            // バウンド計算
            Vector3 center;
            if (useEnemyCenter && enemy_) {
                center = enemy_->GetPosition();
            } else {
                // シーン全体を表示
                center = {0.0f, 3.0f, 15.55f};
            }

            // Agent設定に基づいた範囲計算
            float displayRadius = previewRadius;
            if (useAgentSettings) {
                // Agent Radiusの5倍程度を表示範囲とする
                displayRadius = settings.agentRadius * 5.0f;
                if (displayRadius < 10.0f) displayRadius = 10.0f;
                if (displayRadius > 50.0f) displayRadius = 50.0f;
            }

            Vector3 minBounds = {
                center.x - displayRadius,
                center.y - settings.agentHeight,
                center.z - displayRadius
            };
            Vector3 maxBounds = {
                center.x + displayRadius,
                center.y + settings.agentHeight,
                center.z + displayRadius
            };

            // 毎フレーム更新（Enemyが動いた場合も反映）
            navMeshManager->SetPreviewBounds(minBounds, maxBounds);
            navMeshManager->CreateDebugPreview(engine->GetDXCom(), engine->GetCamera(), settings, enemy_ ? enemy_->GetPosition() : center, showBoundingBox, showGrid);

            // デバッグ情報表示
            ImGui::Text("Debug Info:");
            ImGui::Text("  Preview Enabled: %s", navMeshManager->IsDebugPreviewEnabled() ? "YES" : "NO");
            ImGui::Text("  Center: (%.1f, %.1f, %.1f)", center.x, center.y, center.z);
            ImGui::Text("  Display Radius: %.1f", displayRadius);
            ImGui::Text("  Agent Radius: %.2f", settings.agentRadius);
            ImGui::Text("  Agent Height: %.2f", settings.agentHeight);
            ImGui::Text("  Cell Size: %.3f", settings.cellSize);
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("NavMesh Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Quick Fix: Apply High Precision Preset");
            ImGui::Separator();

            if (ImGui::Button("Apply High Precision Preset")) {
                settings.cellSize = 0.15f;
                settings.cellHeight = 0.2f;
                settings.agentHeight = 2.0f;
                settings.agentRadius = 0.4f;
                settings.agentMaxClimb = 0.5f;
                settings.agentMaxSlope = 45.0f;
                settings.edgeMaxError = 0.8f;
                settings.detailSampleDist = 6.0f;
                AddNavMeshLog("Applied high precision preset");
                settingsChanged = true;
            }

            ImGui::Separator();
            if (ImGui::SliderFloat("Cell Size", &settings.cellSize, 0.05f, 1.0f)) settingsChanged = true;
            if (ImGui::SliderFloat("Cell Height", &settings.cellHeight, 0.05f, 0.5f)) settingsChanged = true;
            if (ImGui::SliderFloat("Agent Height", &settings.agentHeight, 0.5f, 5.0f)) settingsChanged = true;
            if (ImGui::SliderFloat("Agent Radius", &settings.agentRadius, 0.1f, 5.0f)) settingsChanged = true;
            if (ImGui::SliderFloat("Agent Max Climb", &settings.agentMaxClimb, 0.1f, 1.0f)) settingsChanged = true;
            if (ImGui::SliderFloat("Agent Max Slope", &settings.agentMaxSlope, 0.0f, 90.0f)) settingsChanged = true;

            ImGui::Separator();
            ImGui::Text("Corner Smoothness Settings");
            if (ImGui::SliderFloat("Edge Max Error", &settings.edgeMaxError, 0.1f, 3.0f)) settingsChanged = true;
            if (ImGui::SliderFloat("Detail Sample Dist", &settings.detailSampleDist, 1.0f, 10.0f)) settingsChanged = true;

        }

        if (ImGui::Button("Generate NavMesh")) {
            ClearNavMeshLogs();
            AddNavMeshLog("=== Manual NavMesh generation triggered ===");
            engine->GenNav(sceneObjects_, "externals/navimap/stage.navmesh");
            NavMesh* navMesh = navMeshManager->GetNavMesh();
            if (enemy_ && navMesh) {
                enemy_->SetNavMesh(navMesh);
                AddNavMeshLog("NavMesh re-set to Enemy");

                // 新しいAIシステムも更新
                navMeshSystem_ = std::make_unique<NavMeshSystem>();
                if (navMeshSystem_->LoadNavMeshFromFile("externals/navimap/stage.navmesh")) {
                    enemy_->SetNavMeshSystem(navMeshSystem_.get());
                    AddNavMeshLog("New AI System re-initialized");
                }
            }

            // 可視化が有効な場合は次のフレームで更新
            if (engine->IsNavVis()) {
                engine->RequestNavVisUpdate();
                AddNavMeshLog("NavMesh visualization will update next frame");
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Load NavMesh")) {
            ClearNavMeshLogs();
            const std::string navMeshPath = "externals/navimap/stage.navmesh";
            if (engine->LoadNavMesh(navMeshPath)) {
                NavMesh* navMesh = navMeshManager->GetNavMesh();
                if (enemy_ && navMesh) {
                    enemy_->SetNavMesh(navMesh);

                    // 新しいAIシステムも更新
                    navMeshSystem_ = std::make_unique<NavMeshSystem>();
                    if (navMeshSystem_->LoadNavMeshFromFile(navMeshPath)) {
                        enemy_->SetNavMeshSystem(navMeshSystem_.get());
                        AddNavMeshLog("New AI System loaded");
                    }
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Logs")) {
            ClearNavMeshLogs();
        }

        ImGui::Separator();

        // NavMesh情報表示
        NavMesh* navMesh = navMeshManager->GetNavMesh();
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

    // Enemy AI Debug情報
    if (ImGui::CollapsingHeader("Enemy AI Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (enemy_) {
            ImGui::Text("Enemy Position: (%.2f, %.2f, %.2f)",
                enemy_->GetPosition().x,
                enemy_->GetPosition().y,
                enemy_->GetPosition().z);

            ImGui::Text("Using New AI: %s", enemy_->useNewAI_ ? "YES" : "NO");

            if (enemy_->enemyAI_) {
                const char* stateNames[] = { "Idle", "Patrol", "Chase", "Attack", "Search" };
                int stateIndex = static_cast<int>(enemy_->enemyAI_->GetState());
                ImGui::Text("EnemyAI State: %s (%d)",
                    (stateIndex >= 0 && stateIndex < 5) ? stateNames[stateIndex] : "Unknown",
                    stateIndex);
                ImGui::Text("EnemyAI Position: (%.2f, %.2f, %.2f)",
                    enemy_->enemyAI_->GetPosition().x,
                    enemy_->enemyAI_->GetPosition().y,
                    enemy_->enemyAI_->GetPosition().z);
                ImGui::Text("Patrol Mode: %s", enemy_->enemyAI_->IsPatrolModeEnabled() ? "Enabled" : "Disabled");
                ImGui::Text("Target Position: (%.2f, %.2f, %.2f)",
                    enemy_->enemyAI_->GetTargetPosition().x,
                    enemy_->enemyAI_->GetTargetPosition().y,
                    enemy_->enemyAI_->GetTargetPosition().z);
                ImGui::Text("Current Path Size: %d", static_cast<int>(enemy_->enemyAI_->GetCurrentPath().size()));
                ImGui::Text("Current Waypoint: %d", enemy_->enemyAI_->GetCurrentWaypointIndex());

                Vector3 moveDir = enemy_->enemyAI_->GetMoveDirection();
                ImGui::Text("Move Direction: (%.2f, %.2f, %.2f)", moveDir.x, moveDir.y, moveDir.z);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "EnemyAI is NULL!");
            }

            if (navMeshSystem_) {
                ImGui::Text("NavMeshSystem: Valid (%s)", navMeshSystem_->IsValid() ? "OK" : "Invalid");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "NavMeshSystem is NULL!");
            }
        } else {
            ImGui::Text("Enemy: Not Initialized");
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

    if (engine->IsKeyTrig(DIK_F)) {
        lightManager_->ToggleDebugDisplay();
    }

    if (engine->IsKeyTrig(DIK_V)) {
        if (fpsCamera_) {
            bool currentMode = fpsCamera_->IsFPSMode();
            fpsCamera_->SetFPSMode(!currentMode);
        }
    }

    if (engine->IsKeyTrig(DIK_TAB)) {
        if (fpsCamera_ && fpsCamera_->IsFPSMode()) {
            fpsCamera_->ToggleMouseLook();
        }
    }

    // R キーでナビメッシュ再生成
    if (engine->IsKeyTrig(DIK_R)) {
        ClearNavMeshLogs();
        AddNavMeshLog("=== Regenerating NavMesh (R key) ===");
        engine->GenNav(sceneObjects_, "externals/navimap/stage.navmesh");

        // 可視化が有効な場合は次のフレームで更新
        if (engine->IsNavVis()) {
            engine->RequestNavVisUpdate();
            AddNavMeshLog("NavMesh visualization will update next frame");
        }
    }
}

