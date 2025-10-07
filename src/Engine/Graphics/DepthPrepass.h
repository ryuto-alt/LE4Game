#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <functional>

// 深度プリパスレンダラー
// 30-40%のピクセルシェーディングコスト削減を目指す
class DepthPrepassRenderer {
public:
    DepthPrepassRenderer();
    ~DepthPrepassRenderer();

    // 初期化
    void Initialize(ID3D12Device* device, uint32_t width, uint32_t height);

    // 深度専用パイプラインステートを作成
    void CreateDepthOnlyPSO(
        ID3D12RootSignature* rootSignature,
        const D3D12_INPUT_LAYOUT_DESC& inputLayout
    );

    // 深度プリパスを実行
    void ExecuteDepthPrepass(
        ID3D12GraphicsCommandList* cmdList,
        const std::function<void()>& drawCallback
    );

    // 深度バッファを取得
    ID3D12Resource* GetDepthBuffer() const { return depthBuffer_.Get(); }

    // DSVを取得
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return dsvHandle_; }

    // Hi-Zピラミッドを生成(オクルージョンカリング用)
    void GenerateHiZPyramid(ID3D12GraphicsCommandList* cmdList);

    // 統計情報
    struct Statistics {
        uint32_t pixelsSaved = 0; // Early-Zで削減されたピクセル数
        float pixelSavingRate = 0.0f; // %
    };

    const Statistics& GetStatistics() const { return statistics_; }

private:
    ID3D12Device* device_ = nullptr;

    // 深度バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;

    // 深度専用PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> depthOnlyPSO_;

    // Hi-Zピラミッド(ミップチェーン)
    Microsoft::WRL::ComPtr<ID3D12Resource> hiZBuffer_;
    uint32_t hiZMipLevels_ = 0;

    // 解像度
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    // 統計情報
    Statistics statistics_;
};
