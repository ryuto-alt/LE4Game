#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

// メッシュデータ構造
struct Mesh {
    std::vector<float> vertices;  // x,y,z, x,y,z, ...
    std::vector<int> indices;     // triangle indices
};

// glTFローダー
bool LoadGLTF(const std::string& filepath, Mesh& mesh) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = false;
    if (filepath.find(".glb") != std::string::npos) {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    }

    if (!warn.empty()) {
        std::cout << "Warning: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << "Error: " << err << std::endl;
    }

    if (!ret) {
        std::cerr << "Failed to load glTF: " << filepath << std::endl;
        return false;
    }

    // 全メッシュデータを収集
    for (const auto& mesh_obj : model.meshes) {
        for (const auto& primitive : mesh_obj.primitives) {
            // 頂点データの取得
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                const float* positions = reinterpret_cast<const float*>(
                    &buffer.data[bufferView.byteOffset + accessor.byteOffset]);

                size_t vertexCount = accessor.count;
                size_t baseIndex = mesh.vertices.size() / 3;

                for (size_t i = 0; i < vertexCount; ++i) {
                    mesh.vertices.push_back(positions[i * 3 + 0]);
                    mesh.vertices.push_back(positions[i * 3 + 1]);
                    mesh.vertices.push_back(positions[i * 3 + 2]);
                }

                // インデックスデータの取得
                if (primitive.indices >= 0) {
                    const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                    const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
                    const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

                    const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

                    for (size_t i = 0; i < indexAccessor.count; ++i) {
                        int index = 0;
                        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                            index = reinterpret_cast<const unsigned short*>(indexData)[i];
                        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                            index = reinterpret_cast<const unsigned int*>(indexData)[i];
                        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            index = indexData[i];
                        }
                        mesh.indices.push_back(static_cast<int>(baseIndex + index));
                    }
                } else {
                    // インデックスなし（三角形として直接処理）
                    for (size_t i = 0; i < vertexCount; ++i) {
                        mesh.indices.push_back(static_cast<int>(baseIndex + i));
                    }
                }
            }
        }
    }

    std::cout << "Loaded glTF: " << mesh.vertices.size() / 3 << " vertices, "
              << mesh.indices.size() / 3 << " triangles" << std::endl;
    return true;
}

bool SaveNavMesh(const dtNavMesh* navMesh, const std::string& filepath) {
    FILE* fp = nullptr;
    fopen_s(&fp, filepath.c_str(), "wb");
    if (!fp) return false;

    // Magic number and version
    static const int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T';
    static const int NAVMESHSET_VERSION = 1;

    fwrite(&NAVMESHSET_MAGIC, sizeof(int), 1, fp);
    fwrite(&NAVMESHSET_VERSION, sizeof(int), 1, fp);

    // NavMesh parameters
    const dtNavMeshParams* params = navMesh->getParams();
    fwrite(params, sizeof(dtNavMeshParams), 1, fp);

    // Tile count
    int numTiles = 0;
    for (int i = 0; i < navMesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = navMesh->getTile(i);
        if (!tile || !tile->header || !tile->dataSize) continue;
        numTiles++;
    }
    fwrite(&numTiles, sizeof(int), 1, fp);

    // Write tiles
    for (int i = 0; i < navMesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = navMesh->getTile(i);
        if (!tile || !tile->header || !tile->dataSize) continue;

        int tileRef = static_cast<int>(navMesh->getTileRef(tile));
        fwrite(&tileRef, sizeof(int), 1, fp);
        fwrite(&tile->dataSize, sizeof(int), 1, fp);
        fwrite(tile->data, tile->dataSize, 1, fp);
    }

    fclose(fp);
    std::cout << "NavMesh saved: " << numTiles << " tiles" << std::endl;
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: NavMeshGenerator <input.gltf/.glb> <output.bin>" << std::endl;
        std::cout << "Example: NavMeshGenerator level.gltf navmesh.bin" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];

    // Load mesh
    Mesh mesh;
    if (!LoadGLTF(inputPath, mesh)) {
        return 1;
    }

    // Recast configuration (mdファイル推奨値)
    rcConfig cfg = {};
    cfg.cs = 0.2f;                  // Cell size (屋外推奨値)
    cfg.ch = 0.1f;                  // Cell height
    cfg.walkableSlopeAngle = 45.0f; // Max slope angle
    cfg.walkableHeight = 20;        // Agent height in voxels (2.0m / 0.1m)
    cfg.walkableClimb = 3;          // Max step height in voxels (0.3m / 0.1m)
    cfg.walkableRadius = 2;         // Agent radius in voxels (0.4m / 0.2m)
    cfg.maxEdgeLen = 12;            // Max edge length
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea = 8;
    cfg.mergeRegionArea = 20;
    cfg.maxVertsPerPoly = 6;
    cfg.detailSampleDist = 6.0f;
    cfg.detailSampleMaxError = 1.0f;

    // Calculate bounding box
    float bmin[3] = {mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]};
    float bmax[3] = {mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]};

    for (size_t i = 0; i < mesh.vertices.size(); i += 3) {
        bmin[0] = std::min(bmin[0], mesh.vertices[i]);
        bmin[1] = std::min(bmin[1], mesh.vertices[i + 1]);
        bmin[2] = std::min(bmin[2], mesh.vertices[i + 2]);
        bmax[0] = std::max(bmax[0], mesh.vertices[i]);
        bmax[1] = std::max(bmax[1], mesh.vertices[i + 1]);
        bmax[2] = std::max(bmax[2], mesh.vertices[i + 2]);
    }

    rcVcopy(cfg.bmin, bmin);
    rcVcopy(cfg.bmax, bmax);
    rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

    std::cout << "Grid size: " << cfg.width << " x " << cfg.height << std::endl;

    // Build NavMesh
    rcContext ctx;

    rcHeightfield* solid = rcAllocHeightfield();
    if (!rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch)) {
        std::cerr << "Failed to create heightfield" << std::endl;
        return 1;
    }

    std::vector<unsigned char> triAreas(mesh.indices.size() / 3);
    rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, mesh.vertices.data(),
                           mesh.vertices.size() / 3, mesh.indices.data(), mesh.indices.size() / 3, triAreas.data());

    rcRasterizeTriangles(&ctx, mesh.vertices.data(), mesh.vertices.size() / 3,
                        mesh.indices.data(), triAreas.data(), mesh.indices.size() / 3, *solid, cfg.walkableClimb);

    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
    rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

    rcCompactHeightfield* chf = rcAllocCompactHeightfield();
    if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf)) {
        std::cerr << "Failed to build compact heightfield" << std::endl;
        return 1;
    }

    rcFreeHeightField(solid);

    if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf)) {
        std::cerr << "Failed to erode walkable area" << std::endl;
        return 1;
    }

    if (!rcBuildDistanceField(&ctx, *chf)) {
        std::cerr << "Failed to build distance field" << std::endl;
        return 1;
    }

    if (!rcBuildRegions(&ctx, *chf, 0, cfg.minRegionArea, cfg.mergeRegionArea)) {
        std::cerr << "Failed to build regions" << std::endl;
        return 1;
    }

    rcContourSet* cset = rcAllocContourSet();
    if (!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset)) {
        std::cerr << "Failed to build contours" << std::endl;
        return 1;
    }

    rcPolyMesh* pmesh = rcAllocPolyMesh();
    if (!rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh)) {
        std::cerr << "Failed to build poly mesh" << std::endl;
        return 1;
    }

    rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
    if (!rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dmesh)) {
        std::cerr << "Failed to build detail mesh" << std::endl;
        return 1;
    }

    rcFreeCompactHeightfield(chf);
    rcFreeContourSet(cset);

    // Create Detour navmesh
    for (int i = 0; i < pmesh->npolys; ++i) {
        pmesh->flags[i] = 1; // Walkable
    }

    dtNavMeshCreateParams params = {};
    params.verts = pmesh->verts;
    params.vertCount = pmesh->nverts;
    params.polys = pmesh->polys;
    params.polyAreas = pmesh->areas;
    params.polyFlags = pmesh->flags;
    params.polyCount = pmesh->npolys;
    params.nvp = pmesh->nvp;
    params.detailMeshes = dmesh->meshes;
    params.detailVerts = dmesh->verts;
    params.detailVertsCount = dmesh->nverts;
    params.detailTris = dmesh->tris;
    params.detailTriCount = dmesh->ntris;
    params.walkableHeight = 2.0f;
    params.walkableRadius = 0.4f;
    params.walkableClimb = 0.3f;
    rcVcopy(params.bmin, pmesh->bmin);
    rcVcopy(params.bmax, pmesh->bmax);
    params.cs = cfg.cs;
    params.ch = cfg.ch;
    params.buildBvTree = true;

    unsigned char* navData = nullptr;
    int navDataSize = 0;

    if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
        std::cerr << "Failed to create Detour navmesh data" << std::endl;
        return 1;
    }

    dtNavMesh* navMesh = dtAllocNavMesh();
    dtStatus status = navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
    if (dtStatusFailed(status)) {
        std::cerr << "Failed to init navmesh" << std::endl;
        dtFree(navData);
        return 1;
    }

    // Save to file
    if (!SaveNavMesh(navMesh, outputPath)) {
        std::cerr << "Failed to save navmesh" << std::endl;
        return 1;
    }

    std::cout << "NavMesh generated successfully!" << std::endl;
    std::cout << "Output: " << outputPath << std::endl;

    // Cleanup
    rcFreePolyMesh(pmesh);
    rcFreePolyMeshDetail(dmesh);
    dtFreeNavMesh(navMesh);

    return 0;
}
