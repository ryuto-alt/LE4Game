#include "PlacedResourceAllocator.h"
#include <algorithm>
#include <cstdio>

PlacedResourceAllocator::PlacedResourceAllocator() = default;

PlacedResourceAllocator::~PlacedResourceAllocator() {
    // 明示的にリソースを解放
    allocations_.clear();
    heap_.Reset();
}

void PlacedResourceAllocator::Initialize(ID3D12Device* device, size_t heapSize) {
    assert(device);
    device_ = device;
    heapSize_ = heapSize;
    currentOffset_ = 0;

    // ヒープの作成
    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.SizeInBytes = heapSize;
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT; // GPU専用メモリ
    heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapDesc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS; // バッファ専用

    HRESULT hr = device_->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap_));
    assert(SUCCEEDED(hr));

    char debugStr[256];
    sprintf_s(debugStr, "PlacedResourceAllocator: Initialized with %llu MB heap\n", heapSize / (1024 * 1024));
    OutputDebugStringA(debugStr);
}

PlacedResourceAllocator::Allocation PlacedResourceAllocator::Allocate(size_t size, size_t alignment) {
    // アライメント調整
    size_t alignedOffset = AlignUp(currentOffset_, alignment);
    size_t alignedSize = AlignUp(size, alignment);

    // ヒープサイズチェック
    if (alignedOffset + alignedSize > heapSize_) {
        OutputDebugStringA("ERROR: PlacedResourceAllocator: Out of heap memory!\n");
        return Allocation{};
    }

    // Placed Resourceの作成
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = alignedSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Allocation allocation;
    allocation.offset = alignedOffset;
    allocation.size = alignedSize;

    HRESULT hr = device_->CreatePlacedResource(
        heap_.Get(),
        alignedOffset,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON, // 初期状態
        nullptr,
        IID_PPV_ARGS(&allocation.resource)
    );

    if (FAILED(hr)) {
        OutputDebugStringA("ERROR: PlacedResourceAllocator: Failed to create placed resource\n");
        return Allocation{};
    }

    allocation.gpuAddress = allocation.resource->GetGPUVirtualAddress();

    // オフセット更新
    currentOffset_ = alignedOffset + alignedSize;

    // 追跡リストに追加
    allocations_.push_back(allocation);

    char debugStr[256];
    sprintf_s(debugStr, "PlacedResourceAllocator: Allocated %llu MB at offset %llu MB (Total used: %.2f%%)\n",
        alignedSize / (1024 * 1024),
        alignedOffset / (1024 * 1024),
        GetUtilization() * 100.0f
    );
    OutputDebugStringA(debugStr);

    return allocation;
}

void PlacedResourceAllocator::Free(const Allocation& allocation) {
    // アロケーションリストから削除
    auto it = std::find_if(allocations_.begin(), allocations_.end(),
        [&allocation](const Allocation& alloc) {
            return alloc.offset == allocation.offset && alloc.size == allocation.size;
        });

    if (it != allocations_.end()) {
        allocations_.erase(it);
    }

    // 注: 実際のデフラグは Reset() で行う
}

void PlacedResourceAllocator::Reset() {
    allocations_.clear();
    currentOffset_ = 0;
    OutputDebugStringA("PlacedResourceAllocator: Reset complete\n");
}

float PlacedResourceAllocator::GetUtilization() const {
    if (heapSize_ == 0) return 0.0f;
    return static_cast<float>(currentOffset_) / static_cast<float>(heapSize_);
}

size_t PlacedResourceAllocator::AlignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// ===== StagingBufferManager =====

StagingBufferManager::StagingBufferManager() = default;

StagingBufferManager::~StagingBufferManager() {
    if (mappedData_) {
        stagingBuffer_->Unmap(0, nullptr);
        mappedData_ = nullptr;
    }
    stagingBuffer_.Reset();
}

void StagingBufferManager::Initialize(ID3D12Device* device, size_t bufferSize) {
    assert(device);
    device_ = device;
    bufferSize_ = bufferSize;

    // アップロードヒープでステージングバッファを作成
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device_->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&stagingBuffer_)
    );
    assert(SUCCEEDED(hr));

    // 永続的にマップ
    hr = stagingBuffer_->Map(0, nullptr, &mappedData_);
    assert(SUCCEEDED(hr));

    char debugStr[256];
    sprintf_s(debugStr, "StagingBufferManager: Initialized with %llu MB staging buffer\n", bufferSize / (1024 * 1024));
    OutputDebugStringA(debugStr);
}

void* StagingBufferManager::Map(size_t size) {
    if (currentOffset_ + size > bufferSize_) {
        currentOffset_ = 0; // リングバッファのようにラップアラウンド
    }

    void* ptr = static_cast<char*>(mappedData_) + currentOffset_;
    currentOffset_ += size;

    return ptr;
}

void StagingBufferManager::Unmap() {
    // 永続マップなので何もしない
}

void StagingBufferManager::CopyToGPU(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* dest, size_t destOffset, size_t size) {
    assert(cmdList);
    assert(dest);

    cmdList->CopyBufferRegion(dest, destOffset, stagingBuffer_.Get(), 0, size);
}
