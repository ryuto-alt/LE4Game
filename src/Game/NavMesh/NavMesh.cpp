#include "NavMesh.h"
#include "DetourCommon.h"
#include <algorithm>
#define NOMINMAX
#include <Windows.h>

NavMesh::NavMesh()
    : navQuery_(nullptr) {
    builder_ = std::make_unique<NavMeshBuilder>();
}

NavMesh::~NavMesh() {
    if (navQuery_) {
        dtFreeNavMeshQuery(navQuery_);
        navQuery_ = nullptr;
    }
}

void NavMesh::AddModelGeometry(const float* vertices, int vertexCount, const int* indices, int indexCount) {
    if (builder_) {
        builder_->AddGeometry(vertices, vertexCount, indices, indexCount);
    }
}

void NavMesh::ClearGeometry() {
    if (builder_) {
        builder_->ClearGeometry();
    }
}

void NavMesh::SetLogCallback(LogCallback callback) {
    if (builder_) {
        builder_->SetLogCallback(callback);
    }
}

bool NavMesh::InitializeFromGeometry(const NavMeshBuildSettings& settings) {
    if (!builder_) {
        OutputDebugStringA("NavMesh: Builder is null\n");
        return false;
    }

    // ナビメッシュを生成
    if (!builder_->Build(settings)) {
        OutputDebugStringA("NavMesh: Failed to build navmesh\n");
        return false;
    }

    // クエリオブジェクトを作成
    if (navQuery_) {
        dtFreeNavMeshQuery(navQuery_);
    }

    navQuery_ = dtAllocNavMeshQuery();
    if (!navQuery_) {
        OutputDebugStringA("NavMesh: Failed to allocate navmesh query\n");
        return false;
    }

    dtStatus status = navQuery_->init(builder_->GetNavMesh(), 2048);
    if (dtStatusFailed(status)) {
        OutputDebugStringA("NavMesh: Failed to init navmesh query\n");
        dtFreeNavMeshQuery(navQuery_);
        navQuery_ = nullptr;
        return false;
    }

    // フィルター設定
    filter_.setIncludeFlags(0xffff);
    filter_.setExcludeFlags(0);

    OutputDebugStringA("NavMesh: Initialized successfully\n");
    return true;
}

bool NavMesh::SaveToFile(const std::string& filepath) {
    if (!builder_) {
        return false;
    }
    return builder_->SaveToFile(filepath);
}

bool NavMesh::LoadFromFile(const std::string& filepath) {
    if (!builder_) {
        return false;
    }

    if (!builder_->LoadFromFile(filepath)) {
        return false;
    }

    // クエリオブジェクトを再作成
    if (navQuery_) {
        dtFreeNavMeshQuery(navQuery_);
    }

    navQuery_ = dtAllocNavMeshQuery();
    if (!navQuery_) {
        OutputDebugStringA("NavMesh: Failed to allocate navmesh query after loading\n");
        return false;
    }

    dtStatus status = navQuery_->init(builder_->GetNavMesh(), 2048);
    if (dtStatusFailed(status)) {
        OutputDebugStringA("NavMesh: Failed to init navmesh query after loading\n");
        dtFreeNavMeshQuery(navQuery_);
        navQuery_ = nullptr;
        return false;
    }

    // フィルター設定
    filter_.setIncludeFlags(0xffff);
    filter_.setExcludeFlags(0);

    return true;
}

bool NavMesh::FindPath(const float* startPos, const float* endPos, NavMeshPath& outPath) {
    outPath.waypoints.clear();
    outPath.isValid = false;

    if (!navQuery_ || !builder_ || !builder_->GetNavMesh()) {
        OutputDebugStringA("NavMesh: Not initialized for pathfinding\n");
        return false;
    }

    // 開始点と終了点の最近接ポリゴンを検索
    const float extents[3] = { 2.0f, 4.0f, 2.0f }; // 検索範囲
    dtPolyRef startRef = 0;
    dtPolyRef endRef = 0;
    float nearestStartPos[3];
    float nearestEndPos[3];

    navQuery_->findNearestPoly(startPos, extents, &filter_, &startRef, nearestStartPos);
    navQuery_->findNearestPoly(endPos, extents, &filter_, &endRef, nearestEndPos);

    if (!startRef || !endRef) {
        OutputDebugStringA("NavMesh: Could not find start or end poly\n");
        return false;
    }

    // A*パスファインディング
    dtPolyRef polys[MAX_POLYS];
    int npolys = 0;

    navQuery_->findPath(startRef, endRef, nearestStartPos, nearestEndPos, &filter_, polys, &npolys, MAX_POLYS);

    if (npolys == 0) {
        OutputDebugStringA("NavMesh: No path found\n");
        return false;
    }

    // パスをスムージング (Straight path with area crossings for smoother corners)
    float straightPath[MAX_SMOOTH * 3];
    unsigned char straightPathFlags[MAX_SMOOTH];
    dtPolyRef straightPathPolys[MAX_SMOOTH];
    int nstraightPath = 0;

    // DT_STRAIGHTPATH_AREA_CROSSINGS: エリアの境界を通過する際により詳細なポイントを生成
    navQuery_->findStraightPath(nearestStartPos, nearestEndPos, polys, npolys,
        straightPath, straightPathFlags, straightPathPolys,
        &nstraightPath, MAX_SMOOTH, DT_STRAIGHTPATH_AREA_CROSSINGS);

    if (nstraightPath == 0) {
        OutputDebugStringA("NavMesh: Failed to create straight path\n");
        return false;
    }

    // 結果を格納
    outPath.waypoints.resize(nstraightPath * 3);
    std::memcpy(outPath.waypoints.data(), straightPath, nstraightPath * 3 * sizeof(float));
    outPath.isValid = true;

    char msg[256];
    sprintf_s(msg, "NavMesh: Path found with %d waypoints\n", nstraightPath);
    OutputDebugStringA(msg);

    return true;
}

bool NavMesh::Raycast(const Vector3& start, const Vector3& end) {
    if (!navQuery_ || !builder_ || !builder_->GetNavMesh()) {
        return false;  // NavMesh無効時は通る
    }

    float startPos[3] = { start.x, start.y, start.z };
    float endPos[3] = { end.x, end.y, end.z };

    // 開始点の最近接ポリゴンを検索
    const float extents[3] = { 2.0f, 4.0f, 2.0f };
    dtPolyRef startRef = 0;
    float nearestStartPos[3];

    navQuery_->findNearestPoly(startPos, extents, &filter_, &startRef, nearestStartPos);

    if (!startRef) {
        return false;  // 開始点が無効な場合は遮られている
    }

    // レイキャスト実行
    float hitNormal[3];
    dtPolyRef polys[MAX_POLYS];
    int npolys = 0;
    float t = 0;

    dtStatus status = navQuery_->raycast(startRef, startPos, endPos, &filter_,
                                         &t, hitNormal, polys, &npolys, MAX_POLYS);

    // t == 1.0f なら障害物なし（終点まで到達）
    // t < 1.0f なら途中で障害物に当たった
    return (dtStatusSucceed(status) && t >= 1.0f);
}

NavMesh::DebugMeshData NavMesh::GetDebugMeshData() const {
    DebugMeshData data;

    if (!builder_ || !builder_->GetNavMesh()) {
        return data;
    }

    const dtNavMesh* mesh = builder_->GetNavMesh();

    // 全てのタイルをループ
    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = mesh->getTile(i);
        if (!tile || !tile->header) continue;

        // タイル内の全てのポリゴンをループ
        for (int j = 0; j < tile->header->polyCount; ++j) {
            const dtPoly* poly = &tile->polys[j];

            // 三角形に分割
            for (int k = 2; k < poly->vertCount; ++k) {
                int baseIdx = static_cast<int>(data.vertices.size() / 3);

                // 頂点を追加
                for (int v = 0; v < 3; ++v) {
                    int vertIdx = (v == 0) ? 0 : (v == 1) ? (k - 1) : k;
                    const float* vert = &tile->verts[poly->verts[vertIdx] * 3];
                    data.vertices.push_back(vert[0]);
                    data.vertices.push_back(vert[1]);
                    data.vertices.push_back(vert[2]);
                }

                // インデックスを追加
                data.indices.push_back(baseIdx);
                data.indices.push_back(baseIdx + 1);
                data.indices.push_back(baseIdx + 2);
            }
        }
    }

    return data;
}
