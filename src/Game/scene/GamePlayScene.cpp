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
    navMesh_ = std::make_unique<NavMesh>();

    // シーンオブジェクトのロード確認
    char msg[256];
    sprintf_s(msg, "=== Initialize: Scene Objects ===");
    AddNavMeshLog(msg);
    sprintf_s(msg, "Loaded %d scene objects", static_cast<int>(sceneObjects_.size()));
    AddNavMeshLog(msg);
    for (size_t i = 0; i < sceneObjects_.size(); ++i) {
        if (sceneObjects_[i] && sceneObjects_[i]->GetModel()) {
            sprintf_s(msg, "  Object %d: Has model", static_cast<int>(i));
            AddNavMeshLog(msg);
        } else {
            sprintf_s(msg, "  Object %d: No model!", static_cast<int>(i));
            AddNavMeshLog(msg);
        }
    }

    // NavMesh設定のデフォルト値 - 曲がり角を滑らかにするために調整
    navMeshSettings_.cellSize = 0.15f;          // 解像度を上げる（細かく）
    navMeshSettings_.cellHeight = 0.1f;
    navMeshSettings_.agentHeight = 2.0f;
    navMeshSettings_.agentRadius = 1.0f;        // 半径を大きくして角から離れる
    navMeshSettings_.agentMaxClimb = 0.3f;
    navMeshSettings_.agentMaxSlope = 45.0f;
    navMeshSettings_.edgeMaxError = 0.8f;       // エッジエラーを小さく（滑らか）
    navMeshSettings_.detailSampleDist = 3.0f;   // 詳細サンプル距離を小さく

    const std::string navMeshPath = "Resources/NavMesh/stage.navmesh";
    const std::string navMeshDir = "Resources/NavMesh";

    // NavMeshディレクトリを作成（存在しない場合）
    std::filesystem::create_directories(navMeshDir);

    // 保存済みナビメッシュがあれば読み込み、なければ生成
    if (std::filesystem::exists(navMeshPath)) {
        AddNavMeshLog("Loading existing NavMesh...");
        if (navMesh_->LoadFromFile(navMeshPath)) {
            AddNavMeshLog("SUCCESS: NavMesh loaded from file");
        } else {
            AddNavMeshLog("Failed to load NavMesh, regenerating...");
            GenerateAndSaveNavMesh(navMeshPath);
        }
    } else {
        AddNavMeshLog("No existing NavMesh found, generating new one...");
        GenerateAndSaveNavMesh(navMeshPath);
    }

    // EnemyにNavMeshを設定
    if (enemy_ && navMesh_) {
        enemy_->SetNavMesh(navMesh_.get());
        AddNavMeshLog("NavMesh set to Enemy");
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
    ImGui::Begin("Scene Settings");
    ImGui::Text("Camera FOV (degrees): %.2f", sceneData_.camera.fovDegrees);
    ImGui::Text("Camera FOV (radians): %.2f", camera_->GetFovY());
    ImGui::Separator();
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

    // NavMeshデバッグウィンドウ
    ImGui::Begin("NavMesh Debug");
    ImGui::Checkbox("Show NavMesh Debug", &showNavMeshDebug_);

    if (ImGui::CollapsingHeader("NavMesh Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Cell Size", &navMeshSettings_.cellSize, 0.05f, 1.0f);
        ImGui::SliderFloat("Cell Height", &navMeshSettings_.cellHeight, 0.05f, 0.5f);
        ImGui::SliderFloat("Agent Height", &navMeshSettings_.agentHeight, 0.5f, 5.0f);
        ImGui::SliderFloat("Agent Radius", &navMeshSettings_.agentRadius, 0.1f, 2.0f);
        ImGui::SliderFloat("Agent Max Climb", &navMeshSettings_.agentMaxClimb, 0.1f, 1.0f);
        ImGui::SliderFloat("Agent Max Slope", &navMeshSettings_.agentMaxSlope, 0.0f, 90.0f);

        // 曲がり角の滑らかさに影響するパラメータ
        ImGui::Separator();
        ImGui::Text("Corner Smoothness Settings");
        ImGui::SliderFloat("Edge Max Error", &navMeshSettings_.edgeMaxError, 0.1f, 3.0f);
        ImGui::SliderFloat("Detail Sample Dist", &navMeshSettings_.detailSampleDist, 1.0f, 10.0f);
    }

    if (ImGui::Button("Generate NavMesh")) {
        ClearNavMeshLogs();
        AddNavMeshLog("=== Manual NavMesh generation triggered ===");
        GenerateAndSaveNavMesh("Resources/NavMesh/stage.navmesh");
        if (enemy_ && navMesh_) {
            enemy_->SetNavMesh(navMesh_.get());
            AddNavMeshLog("NavMesh re-set to Enemy");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Load NavMesh")) {
        ClearNavMeshLogs();
        const std::string navMeshPath = "Resources/NavMesh/stage.navmesh";
        AddNavMeshLog("=== Loading NavMesh from file ===");
        if (std::filesystem::exists(navMeshPath)) {
            if (navMesh_->LoadFromFile(navMeshPath)) {
                AddNavMeshLog("SUCCESS: NavMesh loaded from file");
                if (enemy_ && navMesh_) {
                    enemy_->SetNavMesh(navMesh_.get());
                }
            } else {
                AddNavMeshLog("ERROR: Failed to load NavMesh");
            }
        } else {
            AddNavMeshLog("ERROR: NavMesh file not found: " + navMeshPath);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Logs")) {
        ClearNavMeshLogs();
    }

    ImGui::Separator();

    // NavMesh情報表示
    if (navMesh_) {
        ImGui::Text("NavMesh Status: %s", navMesh_->IsValid() ? "Valid" : "Invalid");

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
        GenerateAndSaveNavMesh("Resources/NavMesh/stage.navmesh");
    }
}

void GamePlayScene::GenerateAndSaveNavMesh(const std::string& filepath) {
    if (!navMesh_) {
        AddNavMeshLog("ERROR: NavMesh object is null!");
        return;
    }

    char msg[512];
    sprintf_s(msg, "=== Starting NavMesh Generation ===");
    AddNavMeshLog(msg);
    sprintf_s(msg, "Target file: %s", filepath.c_str());
    AddNavMeshLog(msg);

    // 既存のジオメトリをクリア
    navMesh_->ClearGeometry();
    AddNavMeshLog("Cleared existing geometry");

    // シーンの全オブジェクトからジオメトリを抽出
    sprintf_s(msg, "Scene objects count: %d", static_cast<int>(sceneObjects_.size()));
    AddNavMeshLog(msg);

    int totalVertices = 0;
    int totalTriangles = 0;

    for (const auto& obj : sceneObjects_) {
        if (!obj) {
            AddNavMeshLog("  Skipping null object");
            continue;
        }

        // Object3dからModelを取得
        Model* model = obj->GetModel();
        if (!model) {
            AddNavMeshLog("  Skipping object with no model");
            continue;
        }

        const ModelData& modelData = model->GetModelData();

        // Object3dのワールド変換行列を取得
        Matrix4x4 worldMatrix = obj->GetWorldMatrix();

        int objectVertices = 0;
        int objectTriangles = 0;

        // マルチマテリアルデータから取得（GLTFモデル用）
        if (!modelData.matVertexData.empty()) {
            sprintf_s(msg, "  Object has %d material meshes", static_cast<int>(modelData.matVertexData.size()));
            AddNavMeshLog(msg);

            int meshIndex = 0;
            for (const auto& matPair : modelData.matVertexData) {
                const MaterialVertexData& matData = matPair.second;
                const std::vector<VertexData>& vertices = matData.vertices;
                const std::vector<uint32_t>& indices = matData.indices;

                sprintf_s(msg, "    Mesh %d: %d verts, %d indices",
                    meshIndex++,
                    static_cast<int>(vertices.size()),
                    static_cast<int>(indices.size()));
                AddNavMeshLog(msg);

                if (vertices.empty()) {
                    AddNavMeshLog("      -> Empty vertices, skipping");
                    continue;
                }

                // インデックスが空の場合は自動生成（Assimpローダーの場合）
                std::vector<int> intIndices;
                if (indices.empty()) {
                    AddNavMeshLog("      -> No indices, generating from vertices");
                    intIndices.reserve(vertices.size());
                    for (size_t i = 0; i < vertices.size(); ++i) {
                        intIndices.push_back(static_cast<int>(i));
                    }
                } else {
                    intIndices.reserve(indices.size());
                    for (uint32_t idx : indices) {
                        intIndices.push_back(static_cast<int>(idx));
                    }
                }

                // 頂点データをfloat配列に変換（ワールド座標に変換）
                std::vector<float> vertexPositions;
                vertexPositions.reserve(vertices.size() * 3);

                for (const auto& vertex : vertices) {
                    Vector3 localPos(vertex.position.x, vertex.position.y, vertex.position.z);

                    // ワールド座標に変換 (w=1)
                    float x = localPos.x * worldMatrix.m[0][0] + localPos.y * worldMatrix.m[1][0] + localPos.z * worldMatrix.m[2][0] + worldMatrix.m[3][0];
                    float y = localPos.x * worldMatrix.m[0][1] + localPos.y * worldMatrix.m[1][1] + localPos.z * worldMatrix.m[2][1] + worldMatrix.m[3][1];
                    float z = localPos.x * worldMatrix.m[0][2] + localPos.y * worldMatrix.m[1][2] + localPos.z * worldMatrix.m[2][2] + worldMatrix.m[3][2];

                    vertexPositions.push_back(x);
                    vertexPositions.push_back(y);
                    vertexPositions.push_back(z);
                }

                // NavMeshにジオメトリを追加
                navMesh_->AddModelGeometry(
                    vertexPositions.data(),
                    static_cast<int>(vertices.size()),
                    intIndices.data(),
                    static_cast<int>(intIndices.size())
                );

                objectVertices += static_cast<int>(vertices.size());
                objectTriangles += static_cast<int>(intIndices.size()) / 3;
            }
        }
        // 単一メッシュデータから取得（OBJモデル用）
        else {
            const std::vector<VertexData>& vertices = modelData.vertices;
            const std::vector<uint32_t>& indices = modelData.indices;

            sprintf_s(msg, "  Object has %d vertices, %d indices",
                static_cast<int>(vertices.size()),
                static_cast<int>(indices.size()));
            AddNavMeshLog(msg);

            if (vertices.empty() || indices.empty()) {
                AddNavMeshLog("    -> Skipping: empty geometry");
                continue;
            }

            // 頂点データをfloat配列に変換（ワールド座標に変換）
            std::vector<float> vertexPositions;
            vertexPositions.reserve(vertices.size() * 3);

            for (const auto& vertex : vertices) {
                Vector3 localPos(vertex.position.x, vertex.position.y, vertex.position.z);

                // ワールド座標に変換 (w=1)
                float x = localPos.x * worldMatrix.m[0][0] + localPos.y * worldMatrix.m[1][0] + localPos.z * worldMatrix.m[2][0] + worldMatrix.m[3][0];
                float y = localPos.x * worldMatrix.m[0][1] + localPos.y * worldMatrix.m[1][1] + localPos.z * worldMatrix.m[2][1] + worldMatrix.m[3][1];
                float z = localPos.x * worldMatrix.m[0][2] + localPos.y * worldMatrix.m[1][2] + localPos.z * worldMatrix.m[2][2] + worldMatrix.m[3][2];

                vertexPositions.push_back(x);
                vertexPositions.push_back(y);
                vertexPositions.push_back(z);
            }

            // インデックスデータをint配列に変換
            std::vector<int> intIndices;
            intIndices.reserve(indices.size());
            for (uint32_t idx : indices) {
                intIndices.push_back(static_cast<int>(idx));
            }

            // NavMeshにジオメトリを追加
            navMesh_->AddModelGeometry(
                vertexPositions.data(),
                static_cast<int>(vertices.size()),
                intIndices.data(),
                static_cast<int>(indices.size())
            );

            objectVertices += static_cast<int>(vertices.size());
            objectTriangles += static_cast<int>(indices.size()) / 3;
        }

        totalVertices += objectVertices;
        totalTriangles += objectTriangles;

        sprintf_s(msg, "  Added object: %d vertices, %d triangles",
            objectVertices, objectTriangles);
        AddNavMeshLog(msg);
    }

    sprintf_s(msg, "=== Geometry Summary ===");
    AddNavMeshLog(msg);
    sprintf_s(msg, "Total geometry: %d vertices, %d triangles", totalVertices, totalTriangles);
    AddNavMeshLog(msg);

    if (totalVertices == 0 || totalTriangles == 0) {
        AddNavMeshLog("ERROR: No geometry found! Cannot generate NavMesh without geometry.");
        AddNavMeshLog("Please check that sceneObjects_ contains valid models.");
        return;
    }

    // ナビメッシュ設定（ImGuiで設定した値を使用）
    sprintf_s(msg, "=== NavMesh Build Settings ===");
    AddNavMeshLog(msg);
    sprintf_s(msg, "  Cell Size: %.2f, Cell Height: %.2f",
        navMeshSettings_.cellSize, navMeshSettings_.cellHeight);
    AddNavMeshLog(msg);
    sprintf_s(msg, "  Agent: radius=%.2f, height=%.2f, climb=%.2f, slope=%.2f",
        navMeshSettings_.agentRadius, navMeshSettings_.agentHeight,
        navMeshSettings_.agentMaxClimb, navMeshSettings_.agentMaxSlope);
    AddNavMeshLog(msg);

    AddNavMeshLog("=== Building NavMesh ===");
    if (navMesh_->InitializeFromGeometry(navMeshSettings_)) {
        AddNavMeshLog("SUCCESS: NavMesh generated successfully!");

        sprintf_s(msg, "Attempting to save to: %s", filepath.c_str());
        AddNavMeshLog(msg);

        if (navMesh_->SaveToFile(filepath)) {
            AddNavMeshLog("SUCCESS: NavMesh saved to file!");
            sprintf_s(msg, "=== NavMesh Generation Complete ===");
            AddNavMeshLog(msg);
            sprintf_s(msg, "File: %s", filepath.c_str());
            AddNavMeshLog(msg);
        } else {
            AddNavMeshLog("ERROR: Failed to save NavMesh to file!");
            AddNavMeshLog("Check file path and write permissions.");
        }
    } else {
        AddNavMeshLog("ERROR: Failed to generate NavMesh!");
        AddNavMeshLog("Check geometry data and build settings.");
    }
}