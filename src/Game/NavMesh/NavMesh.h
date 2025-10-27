#pragma once
#include "NavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include <vector>
#include <memory>

// パス検索の結果
struct NavMeshPath {
    std::vector<float> waypoints;  // ウェイポイント (x,y,z,x,y,z,...)
    bool isValid = false;

    int GetWaypointCount() const { return static_cast<int>(waypoints.size()) / 3; }

    void GetWaypoint(int index, float& x, float& y, float& z) const {
        if (index >= 0 && index < GetWaypointCount()) {
            x = waypoints[index * 3 + 0];
            y = waypoints[index * 3 + 1];
            z = waypoints[index * 3 + 2];
        }
    }
};

// ナビメッシュシステム
class NavMesh {
public:
    NavMesh();
    ~NavMesh();

    // 初期化: ジオメトリからナビメッシュを生成
    bool InitializeFromGeometry(const NavMeshBuildSettings& settings);

    // モデルからジオメトリを追加
    void AddModelGeometry(const float* vertices, int vertexCount, const int* indices, int indexCount);

    // ジオメトリをクリア
    void ClearGeometry();

    // ナビメッシュの保存・読み込み
    bool SaveToFile(const std::string& filepath);
    bool LoadFromFile(const std::string& filepath);

    // パスファインディング
    bool FindPath(const float* startPos, const float* endPos, NavMeshPath& outPath);

    // ナビメッシュが有効か
    bool IsValid() const { return builder_ && builder_->GetNavMesh() != nullptr; }

    // デバッグ: ビルダーの取得
    NavMeshBuilder* GetBuilder() const { return builder_.get(); }

private:
    std::unique_ptr<NavMeshBuilder> builder_;
    dtNavMeshQuery* navQuery_;
    dtQueryFilter filter_;

    // クエリ用の一時バッファ
    static constexpr int MAX_POLYS = 256;
    static constexpr int MAX_SMOOTH = 2048;
};
