#pragma once
#include <vector>
#include <string>
#include <memory>
#include "Mymath.h"

// ナビメッシュの三角形
struct NavTriangle {
    int indices[3];           // 頂点インデックス
    Vector3 center;           // 三角形の中心
    std::vector<int> neighbors; // 隣接する三角形のインデックス
};

// A*用のノード
struct PathNode {
    int triangleIndex;
    float gCost;  // スタートからのコスト
    float hCost;  // ゴールまでの推定コスト
    float fCost;  // gCost + hCost
    int parentIndex;

    PathNode() : triangleIndex(-1), gCost(0), hCost(0), fCost(0), parentIndex(-1) {}
};

class NavMesh {
public:
    NavMesh();
    ~NavMesh();

    // JSONファイルからナビメッシュを読み込む
    bool LoadFromFile(const std::string& filepath);

    // 2点間のパスを計算（A*アルゴリズム）
    std::vector<Vector3> FindPath(const Vector3& start, const Vector3& end);

    // 指定位置が含まれる三角形を検索
    int FindTriangleAtPosition(const Vector3& position);

    // デバッグ描画用のデータを取得
    const std::vector<Vector3>& GetVertices() const { return vertices_; }
    const std::vector<NavTriangle>& GetTriangles() const { return triangles_; }

private:
    // 点が三角形内にあるかチェック（2D）
    bool IsPointInTriangle(const Vector3& point, int triangleIndex);

    // 2つの三角形の中心間の距離
    float GetDistance(int tri1, int tri2);

    // 三角形の中心からゴールまでの距離（ヒューリスティック）
    float GetHeuristic(int triangleIndex, const Vector3& goal);

    std::vector<Vector3> vertices_;
    std::vector<NavTriangle> triangles_;

    float agentRadius_;
    float agentHeight_;
};
