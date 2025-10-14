#pragma once
#include "NavMeshSystem.h"
#include "EnemyAI.h"
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl/client.h>

using namespace Microsoft::WRL;

// デバッグビジュアライゼーション用の頂点構造
struct DebugVertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

// ナビメッシュデバッグ描画クラス
// mdファイル: デバッグビジュアライゼーション
class NavMeshDebugDraw {
public:
    NavMeshDebugDraw();
    ~NavMeshDebugDraw();

    // 初期化（DirectX12リソース）
    bool Initialize(ID3D12Device* device);

    // ナビメッシュを描画
    void DrawNavMesh(const NavMeshSystem* navMeshSystem);

    // パスを描画
    void DrawPath(const std::vector<Vector3>& path, const DirectX::XMFLOAT4& color);

    // エージェントを描画
    void DrawAgent(const Vector3& position, float radius, const DirectX::XMFLOAT4& color);

    // レンダリング実行
    void Render(ID3D12GraphicsCommandList* commandList);

    // クリア
    void Clear();

    // 有効/無効
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }

private:
    // 線分を追加
    void AddLine(const Vector3& start, const Vector3& end, const DirectX::XMFLOAT4& color);

    // 円を追加
    void AddCircle(const Vector3& center, float radius, const DirectX::XMFLOAT4& color, int segments = 16);

    // 頂点データ
    std::vector<DebugVertex> vertices_;
    std::vector<uint16_t> indices_;

    // DirectX12リソース
    ComPtr<ID3D12Resource> vertexBuffer_;
    ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_;

    // パイプライン（簡易実装用）
    bool initialized_;
    bool enabled_;
    size_t maxVertices_;
    size_t maxIndices_;
};
