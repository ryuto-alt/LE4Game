#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

// Variable Rate Shading (VRS) マネージャ
// 10-20%のGPUパフォーマンス向上を目指す
class VariableRateShading {
public:
    // VRSティア
    enum class VRSTier {
        NotSupported = 0,
        Tier1 = 1, // ドロー単位のシェーディングレート
        Tier2 = 2  // スクリーン空間VRSイメージ + コンビネーター
    };

    // シェーディングレート
    enum class ShadingRate {
        _1X1 = D3D12_SHADING_RATE_1X1, // フル解像度
        _1X2 = D3D12_SHADING_RATE_1X2,
        _2X1 = D3D12_SHADING_RATE_2X1,
        _2X2 = D3D12_SHADING_RATE_2X2, // 2x2ピクセルごとに1回シェーディング
        _2X4 = D3D12_SHADING_RATE_2X4,
        _4X2 = D3D12_SHADING_RATE_4X2,
        _4X4 = D3D12_SHADING_RATE_4X4  // 4x4ピクセルごとに1回シェーディング
    };

    VariableRateShading();
    ~VariableRateShading();

    // 初期化
    void Initialize(ID3D12Device* device);

    // VRSサポート状況をチェック
    VRSTier GetSupportedTier() const { return supportedTier_; }

    // Tier 1: ドロー単位のシェーディングレート設定
    void SetShadingRate(ID3D12GraphicsCommandList* cmdList, ShadingRate rate);

    // Tier 2: スクリーン空間VRSイメージを作成
    void CreateVRSImage(uint32_t width, uint32_t height);

    // Tier 2: VRSイメージを更新(距離ベース)
    void UpdateVRSImageDistanceBased(
        ID3D12GraphicsCommandList* cmdList,
        const float* cameraPosition,
        float maxDistance
    );

    // Tier 2: VRSイメージを更新(速度ベース)
    void UpdateVRSImageVelocityBased(
        ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource* velocityBuffer
    );

    // Tier 2: VRSイメージを適用
    void ApplyVRSImage(ID3D12GraphicsCommandList* cmdList);

    // VRSを無効化
    void DisableVRS(ID3D12GraphicsCommandList* cmdList);

    // 統計情報
    struct Statistics {
        uint32_t pixelsSaved = 0; // VRSで削減されたピクセルシェーディング数
        float performanceGain = 0.0f; // %
    };

    const Statistics& GetStatistics() const { return statistics_; }

private:
    ID3D12Device* device_ = nullptr;
    VRSTier supportedTier_ = VRSTier::NotSupported;

    // Tier 2用のVRSイメージ
    Microsoft::WRL::ComPtr<ID3D12Resource> vrsImage_;
    uint32_t vrsImageWidth_ = 0;
    uint32_t vrsImageHeight_ = 0;

    // 統計情報
    Statistics statistics_;

    // VRSサポート確認
    void CheckVRSSupport();
};
