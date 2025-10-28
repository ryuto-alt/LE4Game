#include "NavMeshManager.h"
#include "UnoEngine.h"
#include "imgui.h"
#include <filesystem>
#include <cmath>

NavMeshManager::NavMeshManager() {
    navMesh_ = std::make_unique<NavMesh>();

    // デフォルト設定
    settings_.cellSize = 0.202f;
    settings_.cellHeight = 0.100f;
    settings_.agentHeight = 2.000f;
    settings_.agentRadius = 2.031f;
    settings_.agentMaxClimb = 0.315f;
    settings_.agentMaxSlope = 45.000f;
    settings_.edgeMaxError = 1.300f;
    settings_.detailSampleDist = 6.080f;
}

NavMeshManager::~NavMeshManager() {
}

void NavMeshManager::Initialize(const std::string& navMeshPath) {
    // NavMeshのログをコールバックに転送
    navMesh_->SetLogCallback([this](const std::string& message) {
        AddLog(message);
    });

    const std::string navMeshDir = "externals/navimap";

    // NavMeshディレクトリを作成（存在しない場合）
    std::error_code ec;
    std::filesystem::create_directories(navMeshDir, ec);
    if (ec) {
        char msg[256];
        sprintf_s(msg, "ERROR: Failed to create NavMesh directory: %s", ec.message().c_str());
        AddLog(msg);
    } else {
        char msg[256];
        sprintf_s(msg, "NavMesh directory ready: %s", navMeshDir.c_str());
        AddLog(msg);
    }

    // 保存済みナビメッシュがあれば読み込み、なければ自動生成は呼び出し側で行う
    if (std::filesystem::exists(navMeshPath)) {
        AddLog("Loading existing NavMesh...");
        if (navMesh_->LoadFromFile(navMeshPath)) {
            AddLog("SUCCESS: NavMesh loaded from file");
        } else {
            AddLog("Failed to load NavMesh from file");
        }
    } else {
        AddLog("No existing NavMesh found");
    }
}

void NavMeshManager::GenerateAndSaveNavMesh(
    const std::vector<std::unique_ptr<Object3d>>& sceneObjects,
    const std::string& filepath
) {
    if (!navMesh_) {
        AddLog("ERROR: NavMesh object is null!");
        return;
    }

    char msg[512];
    sprintf_s(msg, "=== Starting NavMesh Generation ===");
    AddLog(msg);
    sprintf_s(msg, "Target file: %s", filepath.c_str());
    AddLog(msg);

    // 既存のジオメトリをクリア
    navMesh_->ClearGeometry();
    AddLog("Cleared existing geometry");

    // シーンの全オブジェクトからジオメトリを抽出
    sprintf_s(msg, "Scene objects count: %d", static_cast<int>(sceneObjects.size()));
    AddLog(msg);

    int totalVertices = 0;
    int totalTriangles = 0;

    for (const auto& obj : sceneObjects) {
        if (!obj) {
            AddLog("  Skipping null object");
            continue;
        }

        // Object3dからModelを取得
        Model* model = obj->GetModel();
        if (!model) {
            AddLog("  Skipping object with no model");
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
            AddLog(msg);

            int meshIndex = 0;
            for (const auto& matPair : modelData.matVertexData) {
                const MaterialVertexData& matData = matPair.second;
                const std::vector<VertexData>& vertices = matData.vertices;
                const std::vector<uint32_t>& indices = matData.indices;

                sprintf_s(msg, "    Mesh %d: %d verts, %d indices",
                    meshIndex++,
                    static_cast<int>(vertices.size()),
                    static_cast<int>(indices.size()));
                AddLog(msg);

                if (vertices.empty()) {
                    AddLog("      -> Empty vertices, skipping");
                    continue;
                }

                // インデックスが空の場合は自動生成（Assimpローダーの場合）
                std::vector<int> intIndices;
                if (indices.empty()) {
                    AddLog("      -> No indices, generating from vertices");
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
            AddLog(msg);

            if (vertices.empty() || indices.empty()) {
                AddLog("    -> Skipping: empty geometry");
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
        AddLog(msg);
    }

    sprintf_s(msg, "=== Geometry Summary ===");
    AddLog(msg);
    sprintf_s(msg, "Total geometry: %d vertices, %d triangles", totalVertices, totalTriangles);
    AddLog(msg);

    if (totalVertices == 0 || totalTriangles == 0) {
        AddLog("ERROR: No geometry found! Cannot generate NavMesh without geometry.");
        AddLog("Please check that sceneObjects_ contains valid models.");
        return;
    }

    // ナビメッシュ設定
    sprintf_s(msg, "=== NavMesh Build Settings ===");
    AddLog(msg);
    sprintf_s(msg, "  Cell Size: %.2f, Cell Height: %.2f",
        settings_.cellSize, settings_.cellHeight);
    AddLog(msg);
    sprintf_s(msg, "  Agent: radius=%.2f, height=%.2f, climb=%.2f, slope=%.2f",
        settings_.agentRadius, settings_.agentHeight,
        settings_.agentMaxClimb, settings_.agentMaxSlope);
    AddLog(msg);

    AddLog("=== Building NavMesh ===");
    if (navMesh_->InitializeFromGeometry(settings_)) {
        AddLog("SUCCESS: NavMesh generated successfully!");

        sprintf_s(msg, "Attempting to save to: %s", filepath.c_str());
        AddLog(msg);

        if (navMesh_->SaveToFile(filepath)) {
            AddLog("SUCCESS: NavMesh saved to file!");
            sprintf_s(msg, "=== NavMesh Generation Complete ===");
            AddLog(msg);
            sprintf_s(msg, "File: %s", filepath.c_str());
            AddLog(msg);
        } else {
            AddLog("ERROR: Failed to save NavMesh to file!");
            AddLog("Check file path and write permissions.");
        }
    } else {
        AddLog("ERROR: Failed to generate NavMesh!");
        AddLog("Check geometry data and build settings.");
    }
}

bool NavMeshManager::LoadNavMesh(const std::string& filepath) {
    AddLog("=== Loading NavMesh from file ===");
    if (std::filesystem::exists(filepath)) {
        if (navMesh_->LoadFromFile(filepath)) {
            AddLog("SUCCESS: NavMesh loaded from file");
            return true;
        } else {
            AddLog("ERROR: Failed to load NavMesh");
            return false;
        }
    } else {
        AddLog("ERROR: NavMesh file not found: " + filepath);
        return false;
    }
}

void NavMeshManager::Update() {
    // 視覚化オブジェクトの更新
    if (showVisualization_ && visualizationObject_) {
        visualizationObject_->Update();
    }
}

void NavMeshManager::CreateVisualization(DirectXCommon* dxCommon, Camera* camera) {
    dxCommon_ = dxCommon;
    camera_ = camera;

    if (!navMesh_ || !navMesh_->IsValid()) {
        return;
    }

    UnoEngine* unoEngine = UnoEngine::GetInstance();
    visualizationObject_ = unoEngine->CreateObject3D();
    visualizationModel_ = std::make_unique<Model>();

    auto meshData = navMesh_->GetDebugMeshData();
    if (meshData.vertices.empty()) {
        return;
    }

    // ModelDataを構築
    ModelData modelData;
    modelData.vertices.resize(meshData.vertices.size() / 3);
    for (size_t i = 0; i < modelData.vertices.size(); ++i) {
        modelData.vertices[i].position.x = meshData.vertices[i * 3 + 0];
        modelData.vertices[i].position.y = meshData.vertices[i * 3 + 1];
        modelData.vertices[i].position.z = meshData.vertices[i * 3 + 2];
        modelData.vertices[i].position.w = 1.0f;

        modelData.vertices[i].normal.x = 0.0f;
        modelData.vertices[i].normal.y = 0.0f;
        modelData.vertices[i].normal.z = 0.0f;

        modelData.vertices[i].texcoord.x = 0.0f;
        modelData.vertices[i].texcoord.y = 0.0f;
    }
    modelData.indices.assign(meshData.indices.begin(), meshData.indices.end());

    // 各三角形の法線を計算
    for (size_t i = 0; i < modelData.indices.size(); i += 3) {
        uint32_t i0 = modelData.indices[i + 0];
        uint32_t i1 = modelData.indices[i + 1];
        uint32_t i2 = modelData.indices[i + 2];

        Vector3 v0 = {modelData.vertices[i0].position.x, modelData.vertices[i0].position.y, modelData.vertices[i0].position.z};
        Vector3 v1 = {modelData.vertices[i1].position.x, modelData.vertices[i1].position.y, modelData.vertices[i1].position.z};
        Vector3 v2 = {modelData.vertices[i2].position.x, modelData.vertices[i2].position.y, modelData.vertices[i2].position.z};

        Vector3 edge1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
        Vector3 edge2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};

        // 外積で法線を計算
        Vector3 normal = {
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        };

        // 正規化
        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0.0001f) {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }

        // 三角形の各頂点に法線を設定（加算して後で平均化）
        modelData.vertices[i0].normal.x += normal.x;
        modelData.vertices[i0].normal.y += normal.y;
        modelData.vertices[i0].normal.z += normal.z;

        modelData.vertices[i1].normal.x += normal.x;
        modelData.vertices[i1].normal.y += normal.y;
        modelData.vertices[i1].normal.z += normal.z;

        modelData.vertices[i2].normal.x += normal.x;
        modelData.vertices[i2].normal.y += normal.y;
        modelData.vertices[i2].normal.z += normal.z;
    }

    // 法線を正規化
    for (size_t i = 0; i < modelData.vertices.size(); ++i) {
        float length = std::sqrt(
            modelData.vertices[i].normal.x * modelData.vertices[i].normal.x +
            modelData.vertices[i].normal.y * modelData.vertices[i].normal.y +
            modelData.vertices[i].normal.z * modelData.vertices[i].normal.z
        );
        if (length > 0.0001f) {
            modelData.vertices[i].normal.x /= length;
            modelData.vertices[i].normal.y /= length;
            modelData.vertices[i].normal.z /= length;
        } else {
            // フォールバック: 上向き
            modelData.vertices[i].normal.x = 0.0f;
            modelData.vertices[i].normal.y = 1.0f;
            modelData.vertices[i].normal.z = 0.0f;
        }
    }

    // 半透明の緑色マテリアル
    modelData.material.isPBR = true;
    modelData.material.baseColorFactor = {0.0f, 1.0f, 0.0f, 0.3f};  // 緑色半透明
    modelData.material.metallicFactor = 0.0f;
    modelData.material.roughnessFactor = 1.0f;
    modelData.material.doubleSided = true;  // 両面描画

    // Modelを初期化
    visualizationModel_->Initialize(dxCommon_);
    visualizationModel_->GetModelDataInternal() = modelData;
    visualizationModel_->CreateVertexBuffer();

    visualizationObject_->SetModel(visualizationModel_.get());
    visualizationObject_->SetPosition({0.0f, 0.5f, 0.0f});
    visualizationObject_->SetCamera(camera_);
    visualizationObject_->SetEnableLighting(false);
}

void NavMeshManager::DrawVisualization() {
    if (showVisualization_ && visualizationObject_) {
        visualizationObject_->Draw();
    }
}

void NavMeshManager::DrawImGui() {
#ifdef _DEBUG
    ImGui::Begin("NavMesh Manager");

    ImGui::Checkbox("Show NavMesh Visualization", &showVisualization_);

    if (ImGui::CollapsingHeader("NavMesh Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Cell Size", &settings_.cellSize, 0.05f, 1.0f);
        ImGui::SliderFloat("Cell Height", &settings_.cellHeight, 0.05f, 0.5f);
        ImGui::SliderFloat("Agent Height", &settings_.agentHeight, 0.5f, 5.0f);
        ImGui::SliderFloat("Agent Radius", &settings_.agentRadius, 0.1f, 5.0f);
        ImGui::SliderFloat("Agent Max Climb", &settings_.agentMaxClimb, 0.1f, 1.0f);
        ImGui::SliderFloat("Agent Max Slope", &settings_.agentMaxSlope, 0.0f, 90.0f);

        ImGui::Separator();
        ImGui::Text("Corner Smoothness Settings");
        ImGui::SliderFloat("Edge Max Error", &settings_.edgeMaxError, 0.1f, 3.0f);
        ImGui::SliderFloat("Detail Sample Dist", &settings_.detailSampleDist, 1.0f, 10.0f);
    }

    // NavMesh情報表示
    if (navMesh_) {
        ImGui::Separator();
        ImGui::Text("NavMesh Status: %s", navMesh_->IsValid() ? "Valid" : "Invalid");
    } else {
        ImGui::Text("NavMesh: Not Initialized");
    }

    ImGui::End();
#endif
}

void NavMeshManager::AddLog(const std::string& message) {
    if (logCallback_) {
        logCallback_(message);
    }
}
