#include "NavMeshSystem.h"
#include <Recast.h>
#include <DetourCommon.h>
#include <cstring>
#include <cmath>
#include <algorithm>

NavMeshSystem::NavMeshSystem()
    : navMesh_(nullptr)
    , navQuery_(nullptr) {

    // クエリフィルターの初期化
    filter_.setIncludeFlags(0xffff);
    filter_.setExcludeFlags(0);
}

NavMeshSystem::~NavMeshSystem() {
    if (navQuery_) {
        dtFreeNavMeshQuery(navQuery_);
        navQuery_ = nullptr;
    }
    if (navMesh_) {
        dtFreeNavMesh(navMesh_);
        navMesh_ = nullptr;
    }
}

bool NavMeshSystem::LoadNavMeshFromFile(const std::string& filepath) {
    // 既存のナビメッシュをクリア
    if (navQuery_) {
        dtFreeNavMeshQuery(navQuery_);
        navQuery_ = nullptr;
    }
    if (navMesh_) {
        dtFreeNavMesh(navMesh_);
        navMesh_ = nullptr;
    }

    // ファイルからナビメッシュデータを読み込む
    FILE* fp = nullptr;
    errno_t err = fopen_s(&fp, filepath.c_str(), "rb");
    if (err != 0 || !fp) {
        return false;
    }

    // マジックナンバーとバージョンをチェック
    static const int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T';
    static const int NAVMESHSET_VERSION = 1;

    int magic = 0;
    int version = 0;
    fread(&magic, sizeof(int), 1, fp);
    fread(&version, sizeof(int), 1, fp);

    if (magic != NAVMESHSET_MAGIC || version != NAVMESHSET_VERSION) {
        fclose(fp);
        return false;
    }

    // ナビメッシュパラメータを読み込む
    dtNavMeshParams params;
    fread(&params, sizeof(dtNavMeshParams), 1, fp);

    // ナビメッシュを作成
    navMesh_ = dtAllocNavMesh();
    if (!navMesh_) {
        fclose(fp);
        return false;
    }

    dtStatus status = navMesh_->init(&params);
    if (dtStatusFailed(status)) {
        dtFreeNavMesh(navMesh_);
        navMesh_ = nullptr;
        fclose(fp);
        return false;
    }

    // タイル数を読み込む
    int numTiles = 0;
    fread(&numTiles, sizeof(int), 1, fp);

    // 各タイルを読み込む
    for (int i = 0; i < numTiles; ++i) {
        int tileRef = 0;
        int dataSize = 0;

        fread(&tileRef, sizeof(int), 1, fp);
        fread(&dataSize, sizeof(int), 1, fp);

        if (dataSize == 0) continue;

        unsigned char* data = (unsigned char*)dtAlloc(dataSize, DT_ALLOC_PERM);
        if (!data) break;

        fread(data, dataSize, 1, fp);

        navMesh_->addTile(data, dataSize, DT_TILE_FREE_DATA, tileRef, nullptr);
    }

    fclose(fp);

    // ナビメッシュクエリを初期化
    navQuery_ = dtAllocNavMeshQuery();
    if (!navQuery_) {
        dtFreeNavMesh(navMesh_);
        navMesh_ = nullptr;
        return false;
    }

    status = navQuery_->init(navMesh_, 2048);
    if (dtStatusFailed(status)) {
        dtFreeNavMeshQuery(navQuery_);
        dtFreeNavMesh(navMesh_);
        navQuery_ = nullptr;
        navMesh_ = nullptr;
        return false;
    }

    return true;
}

bool NavMeshSystem::GenerateNavMesh(const std::vector<Vector3>& vertices, const std::vector<int>& indices) {
    // この実装は簡略化版
    // 実際のゲームではRecastを使用してより詳細な生成を行う
    // 詳細はmdファイルの「ナビメッシュ生成の5段階アルゴリズム」を参照

    // TODO: Recastを使用した完全な実装
    // 1. ボクセル化
    // 2. フィルタリング
    // 3. リージョン生成
    // 4. コンター抽出
    // 5. ポリゴンメッシュ生成

    return false; // 現在は未実装
}

bool NavMeshSystem::FindPath(const Vector3& start, const Vector3& end, std::vector<Vector3>& outPath) {
    if (!IsValid()) return false;

    outPath.clear();

    // 開始点と終了点をDetour形式に変換
    float startPos[3], endPos[3];
    Vector3TodtVec(start, startPos);
    Vector3TodtVec(end, endPos);

    // 最寄りのポリゴンを検索
    dtPolyRef startRef, endRef;
    float nearestStart[3], nearestEnd[3];

    navQuery_->findNearestPoly(startPos, POLYREF_SEARCH_EXTENT, &filter_, &startRef, nearestStart);
    navQuery_->findNearestPoly(endPos, POLYREF_SEARCH_EXTENT, &filter_, &endRef, nearestEnd);

    if (!startRef || !endRef) {
        return false;
    }

    // A*パスファインディングを実行
    dtPolyRef polys[MAX_POLYS];
    int numPolys = 0;

    dtStatus status = navQuery_->findPath(
        startRef, endRef,
        nearestStart, nearestEnd,
        &filter_,
        polys, &numPolys, MAX_POLYS
    );

    if (dtStatusFailed(status) || numPolys == 0) {
        return false;
    }

    // ストレートパスを取得（ファネリングアルゴリズム適用済み）
    float straightPath[MAX_SMOOTH * 3];
    unsigned char straightPathFlags[MAX_SMOOTH];
    dtPolyRef straightPathPolys[MAX_SMOOTH];
    int straightPathCount = 0;

    status = navQuery_->findStraightPath(
        nearestStart, nearestEnd,
        polys, numPolys,
        straightPath, straightPathFlags, straightPathPolys,
        &straightPathCount, MAX_SMOOTH,
        DT_STRAIGHTPATH_AREA_CROSSINGS
    );

    if (dtStatusFailed(status) || straightPathCount == 0) {
        return false;
    }

    // パスをVector3配列に変換
    for (int i = 0; i < straightPathCount; ++i) {
        outPath.push_back(dtVecToVector3(&straightPath[i * 3]));
    }

    return true;
}

bool NavMeshSystem::GetNearestPoint(const Vector3& position, Vector3& outNearestPoint) {
    if (!IsValid()) return false;

    float pos[3];
    Vector3TodtVec(position, pos);

    dtPolyRef nearestRef;
    float nearestPt[3];

    dtStatus status = navQuery_->findNearestPoly(pos, POLYREF_SEARCH_EXTENT, &filter_, &nearestRef, nearestPt);

    if (dtStatusFailed(status) || !nearestRef) {
        return false;
    }

    outNearestPoint = dtVecToVector3(nearestPt);
    return true;
}

bool NavMeshSystem::IsPointOnNavMesh(const Vector3& position, float maxDistance) {
    if (!IsValid()) return false;

    float pos[3];
    Vector3TodtVec(position, pos);

    float searchExtent[3] = {maxDistance, maxDistance * 2.0f, maxDistance};

    dtPolyRef nearestRef;
    float nearestPt[3];

    dtStatus status = navQuery_->findNearestPoly(pos, searchExtent, &filter_, &nearestRef, nearestPt);

    return dtStatusSucceed(status) && nearestRef != 0;
}

bool NavMeshSystem::Raycast(const Vector3& start, const Vector3& end, Vector3& hitPos) {
    if (!IsValid()) return false;

    float startPos[3], endPos[3];
    Vector3TodtVec(start, startPos);
    Vector3TodtVec(end, endPos);

    dtPolyRef startRef;
    float nearestStart[3];

    navQuery_->findNearestPoly(startPos, POLYREF_SEARCH_EXTENT, &filter_, &startRef, nearestStart);

    if (!startRef) return false;

    float t = 0;
    float hitNormal[3] = {0, 0, 0};
    dtPolyRef polys[MAX_POLYS];
    int numPolys = 0;

    dtStatus status = navQuery_->raycast(
        startRef,
        nearestStart, endPos,
        &filter_,
        &t, hitNormal,
        polys, &numPolys, MAX_POLYS
    );

    if (dtStatusFailed(status)) return false;

    // ヒット位置を計算
    hitPos.x = nearestStart[0] + (endPos[0] - nearestStart[0]) * t;
    hitPos.y = nearestStart[1] + (endPos[1] - nearestStart[1]) * t;
    hitPos.z = nearestStart[2] + (endPos[2] - nearestStart[2]) * t;

    return t < 1.0f; // 1.0未満なら何かにヒット
}

void NavMeshSystem::SmoothPath(std::vector<Vector3>& path) {
    if (path.size() < 3) return;

    // mdファイルのファネリングアルゴリズムは既にfindStraightPathで適用済み
    // ここでは追加のスムージングは不要
    // 必要に応じて、ローカルステアリングレベルでスムージングを適用
}

Vector3 NavMeshSystem::dtVecToVector3(const float* v) const {
    return Vector3{v[0], v[1], v[2]};
}

void NavMeshSystem::Vector3TodtVec(const Vector3& v, float* out) const {
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}
