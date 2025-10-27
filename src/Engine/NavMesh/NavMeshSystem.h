#pragma once
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCrowd.h>
#include <vector>
#include <string>
#include <memory>
#include "../Math/Mymath.h"

// ナビメッシュシステム: Recast/Detourを使用したパスファインディング
class NavMeshSystem {
public:
    NavMeshSystem();
    ~NavMeshSystem();

    // ナビメッシュのロード（シリアライズされたデータから）
    bool LoadNavMeshFromFile(const std::string& filepath);

    // ナビメッシュの生成（ジオメトリから）
    bool GenerateNavMesh(const std::vector<Vector3>& vertices, const std::vector<int>& indices);

    // パスの計算
    bool FindPath(const Vector3& start, const Vector3& end, std::vector<Vector3>& outPath);

    // 最寄りのナビメッシュ上の点を取得
    bool GetNearestPoint(const Vector3& position, Vector3& outNearestPoint);

    // ナビメッシュ上の点かチェック
    bool IsPointOnNavMesh(const Vector3& position, float maxDistance = 2.0f);

    // レイキャスト（壁チェック用）
    bool Raycast(const Vector3& start, const Vector3& end, Vector3& hitPos);

    // ファネリングアルゴリズムでパスをスムージング
    void SmoothPath(std::vector<Vector3>& path);

    // ナビメッシュの有効性チェック
    bool IsValid() const { return navMesh_ != nullptr && navQuery_ != nullptr; }

    // デバッグ情報取得
    const dtNavMesh* GetNavMesh() const { return navMesh_; }
    dtNavMeshQuery* GetNavQuery() const { return navQuery_; }

private:
    // Detourオブジェクト
    dtNavMesh* navMesh_;
    dtNavMeshQuery* navQuery_;
    dtQueryFilter filter_;

    // クエリパラメータ
    static constexpr int MAX_POLYS = 256;
    static constexpr int MAX_SMOOTH = 2048;
    static constexpr float POLYREF_SEARCH_EXTENT[3] = {2.0f, 4.0f, 2.0f};

    // ヘルパー関数
    Vector3 dtVecToVector3(const float* v) const;
    void Vector3TodtVec(const Vector3& v, float* out) const;
};

// エージェントパラメータ（mdファイルより推奨値）
struct NavMeshAgentParams {
    float radius = 0.4f;          // エージェント半径
    float height = 2.0f;          // エージェント高さ
    float maxClimb = 0.3f;        // 登れる段差
    float maxSlope = 45.0f;       // 歩行可能な傾斜（度）
    float walkableHeight = 30;    // ボクセル単位
    float walkableClimb = 5;      // ボクセル単位
    float walkableRadius = 3;     // ボクセル単位
};
