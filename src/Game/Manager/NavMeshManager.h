#pragma once
#include "NavMesh/NavMesh.h"
#include "Object3d.h"
#include "Model.h"
#include "Camera.h"
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

    // ログコールバック
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        logCallback_ = callback;
    }

    // ImGui描画
    void DrawImGui();

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

    // ログコールバック
    std::function<void(const std::string&)> logCallback_;
};
