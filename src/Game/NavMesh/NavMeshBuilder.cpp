#include "NavMeshBuilder.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#define NOMINMAX
#include <Windows.h>

NavMeshBuilder::NavMeshBuilder()
    : navMesh_(nullptr)
    , solid_(nullptr)
    , chf_(nullptr)
    , cset_(nullptr)
    , pmesh_(nullptr)
    , dmesh_(nullptr)
    , logCallback_(nullptr) {
}

void NavMeshBuilder::Log(const std::string& message) {
    if (logCallback_) {
        logCallback_(message);
    }
    OutputDebugStringA((message + "\n").c_str());
}

NavMeshBuilder::~NavMeshBuilder() {
    CleanupIntermediateData();
    CleanupNavMesh();
}

void NavMeshBuilder::AddGeometry(const float* vertices, int vertexCount, const int* indices, int indexCount) {
    // 頂点データを追加
    size_t vertexOffset = inputGeom_.vertices.size();
    inputGeom_.vertices.resize(vertexOffset + vertexCount * 3);
    std::memcpy(&inputGeom_.vertices[vertexOffset], vertices, vertexCount * 3 * sizeof(float));

    // インデックスデータを追加（既存頂点数分オフセット）
    int indexOffset = inputGeom_.GetVertexCount() - vertexCount;
    size_t indexStart = inputGeom_.indices.size();
    inputGeom_.indices.resize(indexStart + indexCount);
    for (int i = 0; i < indexCount; ++i) {
        inputGeom_.indices[indexStart + i] = indices[i] + indexOffset;
    }
}

void NavMeshBuilder::ClearGeometry() {
    inputGeom_.Clear();
}

bool NavMeshBuilder::Build(const NavMeshBuildSettings& settings) {
    if (inputGeom_.GetVertexCount() == 0 || inputGeom_.GetTriangleCount() == 0) {
        OutputDebugStringA("NavMeshBuilder: No input geometry!\n");
        return false;
    }

    // 以前のデータをクリーンアップ
    CleanupIntermediateData();
    CleanupNavMesh();

    // Recast context作成
    rcContext ctx;

    // バウンディングボックスの計算
    float bmin[3], bmax[3];
    bmin[0] = bmin[1] = bmin[2] = FLT_MAX;
    bmax[0] = bmax[1] = bmax[2] = -FLT_MAX;

    for (int i = 0; i < inputGeom_.GetVertexCount(); ++i) {
        const float* v = &inputGeom_.vertices[i * 3];
        bmin[0] = std::min(bmin[0], v[0]);
        bmin[1] = std::min(bmin[1], v[1]);
        bmin[2] = std::min(bmin[2], v[2]);
        bmax[0] = std::max(bmax[0], v[0]);
        bmax[1] = std::max(bmax[1], v[1]);
        bmax[2] = std::max(bmax[2], v[2]);
    }

    // Recast config設定
    rcConfig config;
    std::memset(&config, 0, sizeof(config));

    config.cs = settings.cellSize;
    config.ch = settings.cellHeight;
    config.walkableSlopeAngle = settings.agentMaxSlope;
    config.walkableHeight = static_cast<int>(std::ceil(settings.agentHeight / settings.cellHeight));
    config.walkableClimb = static_cast<int>(std::floor(settings.agentMaxClimb / settings.cellHeight));
    config.walkableRadius = static_cast<int>(std::ceil(settings.agentRadius / settings.cellSize));
    config.maxEdgeLen = static_cast<int>(settings.edgeMaxLen / settings.cellSize);
    config.maxSimplificationError = settings.edgeMaxError;
    config.minRegionArea = settings.regionMinSize * settings.regionMinSize;
    config.mergeRegionArea = settings.regionMergeSize * settings.regionMergeSize;
    config.maxVertsPerPoly = 6;
    config.detailSampleDist = settings.detailSampleDist < 0.9f ? 0 : settings.cellSize * settings.detailSampleDist;
    config.detailSampleMaxError = settings.cellHeight * settings.detailSampleMaxError;

    // バウンディングボックスを設定
    rcVcopy(config.bmin, bmin);
    rcVcopy(config.bmax, bmax);
    rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);

    char msg[512];
    sprintf_s(msg, "NavMesh Build Config:\n"
        "  Grid Size: %d x %d\n"
        "  Cell Size: %.2f, Cell Height: %.2f\n"
        "  Agent: radius=%.2f, height=%.2f, climb=%.2f\n"
        "  Bounds: (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)\n",
        config.width, config.height,
        config.cs, config.ch,
        settings.agentRadius, settings.agentHeight, settings.agentMaxClimb,
        bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2]);
    OutputDebugStringA(msg);

    // Step 1: Rasterize input polygon soup into a voxel grid
    solid_ = rcAllocHeightfield();
    if (!solid_) {
        OutputDebugStringA("NavMeshBuilder: Failed to allocate heightfield\n");
        return false;
    }

    if (!rcCreateHeightfield(&ctx, *solid_, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch)) {
        OutputDebugStringA("NavMeshBuilder: Failed to create heightfield\n");
        return false;
    }

    // Rasterize triangles
    const int ntris = inputGeom_.GetTriangleCount();
    std::vector<unsigned char> triAreas(ntris);
    std::memset(triAreas.data(), 0, ntris * sizeof(unsigned char));

    rcMarkWalkableTriangles(&ctx, config.walkableSlopeAngle,
        inputGeom_.vertices.data(), inputGeom_.GetVertexCount(),
        inputGeom_.indices.data(), ntris,
        triAreas.data());

    rcRasterizeTriangles(&ctx, inputGeom_.vertices.data(), inputGeom_.GetVertexCount(),
        inputGeom_.indices.data(), triAreas.data(), ntris,
        *solid_, config.walkableClimb);

    // Step 2: Filter walkable surfaces
    rcFilterLowHangingWalkableObstacles(&ctx, config.walkableClimb, *solid_);
    rcFilterLedgeSpans(&ctx, config.walkableHeight, config.walkableClimb, *solid_);
    rcFilterWalkableLowHeightSpans(&ctx, config.walkableHeight, *solid_);

    // Step 3: Compact heightfield
    chf_ = rcAllocCompactHeightfield();
    if (!chf_) {
        OutputDebugStringA("NavMeshBuilder: Failed to allocate compact heightfield\n");
        return false;
    }

    if (!rcBuildCompactHeightfield(&ctx, config.walkableHeight, config.walkableClimb, *solid_, *chf_)) {
        OutputDebugStringA("NavMeshBuilder: Failed to build compact heightfield\n");
        return false;
    }

    // Step 4: Erode walkable area
    if (!rcErodeWalkableArea(&ctx, config.walkableRadius, *chf_)) {
        OutputDebugStringA("NavMeshBuilder: Failed to erode walkable area\n");
        return false;
    }

    // Step 5: Build distance field & regions
    if (!rcBuildDistanceField(&ctx, *chf_)) {
        OutputDebugStringA("NavMeshBuilder: Failed to build distance field\n");
        return false;
    }

    if (!rcBuildRegions(&ctx, *chf_, 0, config.minRegionArea, config.mergeRegionArea)) {
        OutputDebugStringA("NavMeshBuilder: Failed to build regions\n");
        return false;
    }

    // Step 6: Build contours
    cset_ = rcAllocContourSet();
    if (!cset_) {
        OutputDebugStringA("NavMeshBuilder: Failed to allocate contour set\n");
        return false;
    }

    if (!rcBuildContours(&ctx, *chf_, config.maxSimplificationError, config.maxEdgeLen, *cset_)) {
        OutputDebugStringA("NavMeshBuilder: Failed to build contours\n");
        return false;
    }

    // Step 7: Build polygon mesh
    pmesh_ = rcAllocPolyMesh();
    if (!pmesh_) {
        OutputDebugStringA("NavMeshBuilder: Failed to allocate poly mesh\n");
        return false;
    }

    if (!rcBuildPolyMesh(&ctx, *cset_, config.maxVertsPerPoly, *pmesh_)) {
        OutputDebugStringA("NavMeshBuilder: Failed to build poly mesh\n");
        return false;
    }

    // Step 8: Build detail mesh
    dmesh_ = rcAllocPolyMeshDetail();
    if (!dmesh_) {
        OutputDebugStringA("NavMeshBuilder: Failed to allocate detail mesh\n");
        return false;
    }

    if (!rcBuildPolyMeshDetail(&ctx, *pmesh_, *chf_, config.detailSampleDist, config.detailSampleMaxError, *dmesh_)) {
        OutputDebugStringA("NavMeshBuilder: Failed to build detail mesh\n");
        return false;
    }

    // Step 9: Create Detour navmesh data
    for (int i = 0; i < pmesh_->npolys; ++i) {
        pmesh_->flags[i] = 1; // walkable
    }

    dtNavMeshCreateParams params;
    std::memset(&params, 0, sizeof(params));
    params.verts = pmesh_->verts;
    params.vertCount = pmesh_->nverts;
    params.polys = pmesh_->polys;
    params.polyAreas = pmesh_->areas;
    params.polyFlags = pmesh_->flags;
    params.polyCount = pmesh_->npolys;
    params.nvp = pmesh_->nvp;
    params.detailMeshes = dmesh_->meshes;
    params.detailVerts = dmesh_->verts;
    params.detailVertsCount = dmesh_->nverts;
    params.detailTris = dmesh_->tris;
    params.detailTriCount = dmesh_->ntris;
    params.walkableHeight = settings.agentHeight;
    params.walkableRadius = settings.agentRadius;
    params.walkableClimb = settings.agentMaxClimb;
    rcVcopy(params.bmin, pmesh_->bmin);
    rcVcopy(params.bmax, pmesh_->bmax);
    params.cs = config.cs;
    params.ch = config.ch;
    params.buildBvTree = true;

    unsigned char* navData = nullptr;
    int navDataSize = 0;

    if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
        OutputDebugStringA("NavMeshBuilder: Failed to create Detour navmesh data\n");
        return false;
    }

    navMesh_ = dtAllocNavMesh();
    if (!navMesh_) {
        OutputDebugStringA("NavMeshBuilder: Failed to allocate navmesh\n");
        dtFree(navData);
        return false;
    }

    dtStatus status = navMesh_->init(navData, navDataSize, DT_TILE_FREE_DATA);
    if (dtStatusFailed(status)) {
        OutputDebugStringA("NavMeshBuilder: Failed to init navmesh\n");
        dtFree(navData);
        return false;
    }

    sprintf_s(msg, "NavMesh built successfully!\n  Polys: %d, Verts: %d\n", pmesh_->npolys, pmesh_->nverts);
    OutputDebugStringA(msg);

    return true;
}

bool NavMeshBuilder::SaveToFile(const std::string& filepath) const {
    if (!navMesh_) {
        const_cast<NavMeshBuilder*>(this)->Log("NavMeshBuilder: No navmesh to save");
        return false;
    }

    // ファイルパスのディレクトリ部分を確認
    char msg[512];
    sprintf_s(msg, "NavMeshBuilder: Attempting to save to: %s", filepath.c_str());
    const_cast<NavMeshBuilder*>(this)->Log(msg);

    // ファイルをバイナリモードで開く（既存ファイルは上書き）
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        sprintf_s(msg, "NavMeshBuilder: Failed to open file for writing: %s", filepath.c_str());
        const_cast<NavMeshBuilder*>(this)->Log(msg);
        return false;
    }

    // Get tile data
    const dtNavMesh* mesh = navMesh_;
    int tileCount = 0;
    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = mesh->getTile(i);
        if (!tile || !tile->header || !tile->dataSize) continue;

        // Write tile data size
        file.write(reinterpret_cast<const char*>(&tile->dataSize), sizeof(tile->dataSize));
        // Write tile data
        file.write(reinterpret_cast<const char*>(tile->data), tile->dataSize);
        tileCount++;
    }

    file.close();

    sprintf_s(msg, "NavMeshBuilder: Saved %d tiles to file: %s", tileCount, filepath.c_str());
    const_cast<NavMeshBuilder*>(this)->Log(msg);

    // ファイルが実際に作成されたか確認
    std::ifstream checkFile(filepath, std::ios::binary);
    if (checkFile.is_open()) {
        checkFile.seekg(0, std::ios::end);
        size_t fileSize = checkFile.tellg();
        checkFile.close();
        sprintf_s(msg, "NavMeshBuilder: File created successfully, size: %zu bytes", fileSize);
        const_cast<NavMeshBuilder*>(this)->Log(msg);
        return true;
    } else {
        const_cast<NavMeshBuilder*>(this)->Log("NavMeshBuilder: File was not created!");
        return false;
    }
}

bool NavMeshBuilder::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        OutputDebugStringA("NavMeshBuilder: Failed to open file for reading\n");
        return false;
    }

    CleanupNavMesh();

    navMesh_ = dtAllocNavMesh();
    if (!navMesh_) {
        OutputDebugStringA("NavMeshBuilder: Failed to allocate navmesh\n");
        return false;
    }

    // Read tile data
    while (file.peek() != EOF) {
        int dataSize = 0;
        file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
        if (dataSize <= 0) break;

        unsigned char* data = static_cast<unsigned char*>(dtAlloc(dataSize, DT_ALLOC_PERM));
        if (!data) {
            OutputDebugStringA("NavMeshBuilder: Failed to allocate tile data\n");
            return false;
        }

        file.read(reinterpret_cast<char*>(data), dataSize);

        navMesh_->init(data, dataSize, DT_TILE_FREE_DATA);
    }

    file.close();
    OutputDebugStringA("NavMesh loaded from file\n");
    return true;
}

void NavMeshBuilder::CleanupIntermediateData() {
    rcFreeHeightField(solid_);
    solid_ = nullptr;

    rcFreeCompactHeightfield(chf_);
    chf_ = nullptr;

    rcFreeContourSet(cset_);
    cset_ = nullptr;

    rcFreePolyMesh(pmesh_);
    pmesh_ = nullptr;

    rcFreePolyMeshDetail(dmesh_);
    dmesh_ = nullptr;
}

void NavMeshBuilder::CleanupNavMesh() {
    dtFreeNavMesh(navMesh_);
    navMesh_ = nullptr;
}
