#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>

class DirectXCommon;

// SRVを管理するクラス
class SrvManager {
public:
    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // SRVの確保
    uint32_t Allocate();

    // CPUハンドルの取得
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

    // GPUハンドルの取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    // SRV生成関数（テクスチャ用）
    void CreateSRVForTexture2D(uint32_t srvIndex, Microsoft::WRL::ComPtr<ID3D12Resource> pResource, DXGI_FORMAT format, UINT mipLevels);

    // SRV生成関数（Cubemap用）
    void CreateSRVForTextureCube(uint32_t srvIndex, Microsoft::WRL::ComPtr<ID3D12Resource> pResource, DXGI_FORMAT format, UINT mipLevels);

    // SRV生成関数（Structured Buffer用）
    void CreateSRVForStructuredBuffer(uint32_t srvIndex, Microsoft::WRL::ComPtr<ID3D12Resource> pResource, UINT numElements, UINT structureByteStride);

    // ヒープセットコマンド（描画前処理）
    void PreDraw();

    // SRVセットコマンド
    void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

    // 最大数チェック
    bool IsMaxCount();

    // SRVディスクリプタヒープの取得（ImGui用）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() const { return descriptorHeap; }

private:
    // DirectXCommon
    DirectXCommon* dxCommon_ = nullptr;

    // 次に使用するSRVインデックス
    uint32_t useIndex_ = 0;

    // SRVディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;

    // SRVディスクリプタサイズ
    uint32_t descriptorSize = 0;

    // 最大SRVカウント
    static const uint32_t kMaxSRVCount = 512;
};