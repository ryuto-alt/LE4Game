#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <cassert>
#include <cstdint>

// Placed Resources による大規模頂点バッファ管理
// 5860万頂点(約2.2GB)を効率的に配置するためのアロケータ
class PlacedResourceAllocator {
public:
    // ヒープサイズ(8GB: 大規模モデル対応)
    static constexpr size_t kDefaultHeapSize = 8ULL * 1024ULL * 1024ULL * 1024ULL; // 8GB

    // アロケーション情報
    struct Allocation {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        size_t offset = 0;
        size_t size = 0;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
    };

    PlacedResourceAllocator();
    ~PlacedResourceAllocator();

    // 初期化
    void Initialize(ID3D12Device* device, size_t heapSize = kDefaultHeapSize);

    // バッファをアロケート
    Allocation Allocate(size_t size, size_t alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

    // 解放(デフラグ用)
    void Free(const Allocation& allocation);

    // リセット(すべて解放)
    void Reset();

    // 使用率を取得
    float GetUtilization() const;

    // 利用可能メモリを取得
    size_t GetAvailableMemory() const { return heapSize_ - currentOffset_; }

private:
    // デバイス
    ID3D12Device* device_ = nullptr;

    // メインヒープ(DEFAULT heap)
    Microsoft::WRL::ComPtr<ID3D12Heap> heap_;
    size_t heapSize_ = 0;
    size_t currentOffset_ = 0;

    // アロケーション追跡
    std::vector<Allocation> allocations_;

    // アライメント調整
    size_t AlignUp(size_t value, size_t alignment);
};

// GPUアップロード用のステージングバッファマネージャ
class StagingBufferManager {
public:
    StagingBufferManager();
    ~StagingBufferManager();

    void Initialize(ID3D12Device* device, size_t bufferSize = 512ULL * 1024ULL * 1024ULL); // 512MB

    // データをステージング
    void* Map(size_t size);
    void Unmap();

    // ステージングバッファからGPUへコピー
    void CopyToGPU(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* dest, size_t destOffset, size_t size);

    ID3D12Resource* GetResource() const { return stagingBuffer_.Get(); }

private:
    ID3D12Device* device_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer_;
    void* mappedData_ = nullptr;
    size_t bufferSize_ = 0;
    size_t currentOffset_ = 0;
};
