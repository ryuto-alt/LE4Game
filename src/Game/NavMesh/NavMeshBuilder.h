#pragma once
#include <vector>
#include <string>
#include <functional>
#include "Recast.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"

// ナビメッシュ生成のためのパラメータ
struct NavMeshBuildSettings {
    // Cell size (ボクセルの横幅)
    // 推奨: agent_radius / 2 (屋外) または agent_radius / 3 (屋内)
    float cellSize = 0.2f;

    // Cell height (ボクセルの高さ)
    // 推奨: cellSize / 2
    float cellHeight = 0.1f;

    // Agent parameters (Enemy scale 3.0を考慮)
    float agentHeight = 2.0f;      // エージェントの高さ
    float agentRadius = 0.5f;      // エージェントの半径 (スケール3.0を考慮して少し大きめ)
    float agentMaxClimb = 0.3f;    // 登れる段差の高さ
    float agentMaxSlope = 45.0f;   // 登れる坂の最大角度(度)

    // Region settings
    int regionMinSize = 8;         // 最小リージョンサイズ (小さい孤立領域を除去)
    int regionMergeSize = 400;     // リージョンマージサイズ

    // Edge settings
    float edgeMaxLen = 12.0f;      // 最大エッジ長
    float edgeMaxError = 1.3f;     // エッジ単純化エラー

    // Detail mesh settings
    float detailSampleDist = 6.0f;  // 詳細メッシュサンプル距離
    float detailSampleMaxError = 1.0f;  // 詳細メッシュ最大エラー
};

// 入力ジオメトリ
struct InputGeometry {
    std::vector<float> vertices;  // 頂点データ (x,y,z,x,y,z,...)
    std::vector<int> indices;     // インデックスデータ (三角形)

    void Clear() {
        vertices.clear();
        indices.clear();
    }

    int GetVertexCount() const { return static_cast<int>(vertices.size()) / 3; }
    int GetTriangleCount() const { return static_cast<int>(indices.size()) / 3; }
};

// ナビメッシュビルダー
class NavMeshBuilder {
public:
    NavMeshBuilder();
    ~NavMeshBuilder();

    // ジオメトリの追加
    void AddGeometry(const float* vertices, int vertexCount, const int* indices, int indexCount);
    void ClearGeometry();

    // ナビメッシュの生成
    bool Build(const NavMeshBuildSettings& settings);

    // ナビメッシュの取得
    dtNavMesh* GetNavMesh() const { return navMesh_; }

    // ナビメッシュの保存・読み込み
    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);

    // デバッグ情報取得
    const InputGeometry& GetInputGeometry() const { return inputGeom_; }

    // ログコールバック設定
    using LogCallback = std::function<void(const std::string&)>;
    void SetLogCallback(LogCallback callback) { logCallback_ = callback; }

private:
    void Log(const std::string& message);
    LogCallback logCallback_;
    InputGeometry inputGeom_;
    dtNavMesh* navMesh_;

    // Recast intermediate data
    rcHeightfield* solid_;
    rcCompactHeightfield* chf_;
    rcContourSet* cset_;
    rcPolyMesh* pmesh_;
    rcPolyMeshDetail* dmesh_;

    void CleanupIntermediateData();
    void CleanupNavMesh();
};
