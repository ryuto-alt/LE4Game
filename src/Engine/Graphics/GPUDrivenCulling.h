#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <cstdint>
#include "Mymath.h"

// GPU駆動カリングシステム(ExecuteIndirect)
// 5860万頂点から実際に描画する頂点を40-60%削減
class GPUDrivenCulling {
public:
    // フラスタムカリング用の定数
    struct CullingConstants {
        Matrix4x4 viewProjection;
        Vector4 frustumPlanes[6]; // 6つのフラスタム平面
        Vector3 cameraPosition;
        float nearPlane;
        float farPlane;
        uint32_t totalDrawCalls;
        uint32_t padding[2];
    };

    // ドローコール情報(GPU側で生成)
    struct DrawCommand {
        D3D12_DRAW_ARGUMENTS drawArgs;
        uint32_t visible; // 0 or 1
    };

    // オブジェクト情報(バウンディングボックス)
    struct ObjectData {
        Vector3 boundsMin;
        float padding1;
        Vector3 boundsMax;
        float padding2;
        Matrix4x4 worldMatrix;
    };

    GPUDrivenCulling();
    ~GPUDrivenCulling();

    // 初期化
    void Initialize(ID3D12Device* device, uint32_t maxDrawCalls = 10000);

    // オブジェクトを登録
    void RegisterObject(const ObjectData& object, const D3D12_DRAW_ARGUMENTS& drawArgs);

    // フラスタムカリングを実行(GPU側)
    void ExecuteCulling(
        ID3D12GraphicsCommandList* cmdList,
        const Matrix4x4& viewProjection,
        const Vector3& cameraPosition,
        float nearPlane,
        float farPlane
    );

    // カリング済みドローコールを実行
    void ExecuteIndirectDraw(ID3D12GraphicsCommandList* cmdList, ID3D12PipelineState* pso);

    // 統計情報
    struct Statistics {
        uint32_t totalObjects = 0;
        uint32_t visibleObjects = 0;
        uint32_t culledObjects = 0;
        float cullingRate = 0.0f; // %
    };

    const Statistics& GetStatistics() const { return statistics_; }

    // リセット(次フレーム用)
    void Reset();

private:
    // デバイス
    ID3D12Device* device_ = nullptr;

    // コンピュートシェーダー用リソース
    Microsoft::WRL::ComPtr<ID3D12RootSignature> cullingRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> cullingPSO_;

    // バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> objectDataBuffer_;      // オブジェクト情報
    Microsoft::WRL::ComPtr<ID3D12Resource> drawCommandBuffer_;     // ドローコマンド(GPU書き込み)
    Microsoft::WRL::ComPtr<ID3D12Resource> cullingConstantsBuffer_; // カリング定数
    Microsoft::WRL::ComPtr<ID3D12Resource> countBuffer_;           // 可視オブジェクト数

    // ExecuteIndirect用
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;

    // デスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvUavHeap_;

    // オブジェクトデータ
    std::vector<ObjectData> objects_;
    std::vector<D3D12_DRAW_ARGUMENTS> drawArgs_;

    // 統計情報
    Statistics statistics_;

    // 最大ドローコール数
    uint32_t maxDrawCalls_ = 0;

    // フラスタム平面を計算
    void CalculateFrustumPlanes(const Matrix4x4& viewProjection, Vector4 planes[6]);

    // シェーダーをコンパイル
    void CreateCullingShader();
};
