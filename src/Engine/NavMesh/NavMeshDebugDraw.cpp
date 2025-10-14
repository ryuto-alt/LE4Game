#include "NavMeshDebugDraw.h"
#include <DetourNavMesh.h>
#include <cmath>

NavMeshDebugDraw::NavMeshDebugDraw()
    : initialized_(false)
    , enabled_(true)
    , maxVertices_(10000)
    , maxIndices_(20000) {
}

NavMeshDebugDraw::~NavMeshDebugDraw() {
}

bool NavMeshDebugDraw::Initialize(ID3D12Device* device) {
    if (!device) return false;

    // 頂点バッファとインデックスバッファの初期化
    // 実際のDirectX12実装では、ヒーププロパティやリソース記述子を設定
    // ここでは簡略化のため基本構造のみ

    initialized_ = true;
    return true;
}

void NavMeshDebugDraw::DrawNavMesh(const NavMeshSystem* navMeshSystem) {
    if (!enabled_ || !navMeshSystem || !navMeshSystem->IsValid()) {
        return;
    }

    const dtNavMesh* navMesh = navMeshSystem->GetNavMesh();
    if (!navMesh) return;

    // ナビメッシュのタイルを走査
    for (int i = 0; i < navMesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = navMesh->getTile(i);
        if (!tile || !tile->header) continue;

        // ポリゴンを描画
        for (int j = 0; j < tile->header->polyCount; ++j) {
            const dtPoly* poly = &tile->polys[j];

            // ポリゴンの頂点数
            int vertCount = poly->vertCount;

            // ポリゴンのエッジを描画
            for (int k = 0; k < vertCount; ++k) {
                int v0 = poly->verts[k];
                int v1 = poly->verts[(k + 1) % vertCount];

                const float* p0 = &tile->verts[v0 * 3];
                const float* p1 = &tile->verts[v1 * 3];

                Vector3 start = {p0[0], p0[1], p0[2]};
                Vector3 end = {p1[0], p1[1], p1[2]};

                // ナビメッシュは緑色
                AddLine(start, end, {0.0f, 1.0f, 0.0f, 1.0f});
            }
        }
    }
}

void NavMeshDebugDraw::DrawPath(const std::vector<Vector3>& path, const DirectX::XMFLOAT4& color) {
    if (!enabled_ || path.size() < 2) {
        return;
    }

    // パスの線を描画
    for (size_t i = 0; i < path.size() - 1; ++i) {
        AddLine(path[i], path[i + 1], color);
    }

    // ウェイポイントを円で描画
    for (const auto& waypoint : path) {
        AddCircle(waypoint, 0.3f, color, 8);
    }
}

void NavMeshDebugDraw::DrawAgent(const Vector3& position, float radius, const DirectX::XMFLOAT4& color) {
    if (!enabled_) {
        return;
    }

    // エージェントを円で表示
    AddCircle(position, radius, color, 16);

    // 前方方向を示す線（簡易実装）
    Vector3 forward = {position.x, position.y, position.z + radius * 1.5f};
    AddLine(position, forward, color);
}

void NavMeshDebugDraw::Render(ID3D12GraphicsCommandList* commandList) {
    if (!enabled_ || !initialized_ || vertices_.empty()) {
        return;
    }

    // DirectX12でのレンダリング実装
    // 実際のプロジェクトでは、パイプラインステート、ルートシグネチャ、
    // シェーダーなどを設定してDrawInstanced()を呼び出す

    // 簡易実装: ここでは頂点データの準備のみ
    // 実際の描画は既存のエンジンのレンダリングシステムに統合する必要がある
}

void NavMeshDebugDraw::Clear() {
    vertices_.clear();
    indices_.clear();
}

void NavMeshDebugDraw::AddLine(const Vector3& start, const Vector3& end, const DirectX::XMFLOAT4& color) {
    if (vertices_.size() + 2 > maxVertices_) {
        return; // バッファオーバーフロー防止
    }

    uint16_t startIndex = static_cast<uint16_t>(vertices_.size());

    DebugVertex v0;
    v0.position = {start.x, start.y, start.z};
    v0.color = color;
    vertices_.push_back(v0);

    DebugVertex v1;
    v1.position = {end.x, end.y, end.z};
    v1.color = color;
    vertices_.push_back(v1);

    // ラインリスト用のインデックス
    indices_.push_back(startIndex);
    indices_.push_back(startIndex + 1);
}

void NavMeshDebugDraw::AddCircle(const Vector3& center, float radius, const DirectX::XMFLOAT4& color, int segments) {
    if (vertices_.size() + segments > maxVertices_) {
        return;
    }

    float angleStep = 3.14159265f * 2.0f / segments;

    for (int i = 0; i < segments; ++i) {
        float angle0 = angleStep * i;
        float angle1 = angleStep * ((i + 1) % segments);

        Vector3 p0 = {
            center.x + std::cos(angle0) * radius,
            center.y,
            center.z + std::sin(angle0) * radius
        };

        Vector3 p1 = {
            center.x + std::cos(angle1) * radius,
            center.y,
            center.z + std::sin(angle1) * radius
        };

        AddLine(p0, p1, color);
    }
}
