#pragma once
#include "DirectXCommon.h"
#include "Camera.h"
#include "Mymath.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>

using namespace Microsoft::WRL;

// ライン描画用の頂点構造体
struct LineVertex {
    Vector4 position;  // xyz + w
    Vector4 color;     // rgba
};

// ライン描画専用クラス
class LineRenderer {
public:
    LineRenderer();
    ~LineRenderer();

    void Initialize(DirectXCommon* dxCommon, Camera* camera);
    void AddLine(const Vector3& start, const Vector3& end, const Vector4& color);
    void Clear();
    void Render();

private:
    void CreatePipelineState();
    void CreateBuffers();

    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;

    std::vector<LineVertex> vertices_;

    ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    ComPtr<ID3D12Resource> transformBuffer_;
    Matrix4x4* transformData_ = nullptr;

    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;

    static constexpr UINT kMaxLines = 100000;  // 最大ライン数
};
