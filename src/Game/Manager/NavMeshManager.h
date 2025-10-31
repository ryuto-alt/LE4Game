#pragma once
#include "NavMesh/NavMesh.h"
#include "Object3d.h"
#include "Model.h"
#include "Camera.h"
#include "LineRenderer.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

class NavMeshManager {
public:
    NavMeshManager();
    ~NavMeshManager();

    // 初期化
    void Initialize(const std::string& navMeshPath);

    // NavMeshの生成と保存
    void GenerateAndSaveNavMesh(
        const std::vector<std::unique_ptr<Object3d>>& sceneObjects,
        const std::string& filepath
    );

    // NavMeshの読み込み
    bool LoadNavMesh(const std::string& filepath);

    // 設定の保存・読み込み
    void SaveSettings(const std::string& filepath);
    bool LoadSettings(const std::string& filepath);

    // 更新
    void Update();

    // NavMesh取得
    NavMesh* GetNavMesh() const { return navMesh_.get(); }

    // 設定
    NavMeshBuildSettings& GetSettings() { return settings_; }
    const NavMeshBuildSettings& GetSettings() const { return settings_; }
    void SetSettings(const NavMeshBuildSettings& settings) { settings_ = settings; }

    // 視覚化
    void SetVisualizationEnabled(bool enabled) { showVisualization_ = enabled; }
    bool IsVisualizationEnabled() const { return showVisualization_; }
    void DrawVisualization();
    void CreateVisualization(DirectXCommon* dxCommon, Camera* camera);
    void RequestVisualizationUpdate() { needsVisualizationUpdate_ = true; }

    // デバッグプレビュー
    void SetDebugPreviewEnabled(bool enabled) { showDebugPreview_ = enabled; }
    bool IsDebugPreviewEnabled() const { return showDebugPreview_; }
    void DrawDebugPreview();
    void CreateDebugPreview(DirectXCommon* dxCommon, Camera* camera, const NavMeshBuildSettings& settings, const Vector3& agentPosition = {0, 0, 0}, bool showBoundingBox = true, bool showGrid = true);
    void SetPreviewBounds(const Vector3& min, const Vector3& max);

    // ログコールバック
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        logCallback_ = callback;
    }

    // ImGui描画
    void DrawImGui();

    // デバッグ表示の切り替え
    void ToggleDebugDisplay() { showDebugWindow_ = !showDebugWindow_; }
    bool IsDebugDisplayShown() const { return showDebugWindow_; }

private:
    void AddLog(const std::string& message);

    std::unique_ptr<NavMesh> navMesh_;
    NavMeshBuildSettings settings_;

    // 視覚化用
    bool showVisualization_ = false;
    bool needsVisualizationUpdate_ = false;  // 可視化の更新が必要かどうか
    std::unique_ptr<Object3d> visualizationObject_;
    std::unique_ptr<Model> visualizationModel_;
    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;

    // デバッグプレビュー用
    bool showDebugPreview_ = false;
    std::unique_ptr<LineRenderer> lineRenderer_;
    Vector3 previewBoundsMin_{-100.0f, 0.0f, -100.0f};
    Vector3 previewBoundsMax_{100.0f, 10.0f, 100.0f};

    // ログコールバック
    std::function<void(const std::string&)> logCallback_;

    // ImGuiデバッグウィンドウ表示フラグ
    bool showDebugWindow_ = false;
};
