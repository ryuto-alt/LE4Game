#include "PerformanceProfiler.h"
#include <cassert>
#include <algorithm>
#include <cstdio>

PerformanceProfiler::PerformanceProfiler() = default;

PerformanceProfiler::~PerformanceProfiler() {
    queryHeap_.Reset();
    queryResultBuffer_.Reset();
}

void PerformanceProfiler::Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue) {
    assert(device);
    assert(commandQueue);

    device_ = device;
    commandQueue_ = commandQueue;

    // タイムスタンプクエリヒープ作成
    D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = kMaxQueries;
    queryHeapDesc.NodeMask = 0;

    HRESULT hr = device_->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap_));
    assert(SUCCEEDED(hr));

    // クエリ結果バッファ
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(uint64_t) * kMaxQueries;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&queryResultBuffer_)
    );
    assert(SUCCEEDED(hr));

    // GPU周波数取得
    commandQueue_->GetTimestampFrequency(&gpuFrequency_);

    OutputDebugStringA("PerformanceProfiler: Initialized\n");
    char debugStr[256];
    sprintf_s(debugStr, "PerformanceProfiler: GPU frequency: %llu Hz\n", gpuFrequency_);
    OutputDebugStringA(debugStr);
}

void PerformanceProfiler::BeginFrame() {
    frameStartTime_ = std::chrono::high_resolution_clock::now();
    queryIndex_ = 0;
    scopes_.clear();
}

void PerformanceProfiler::EndFrame() {
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    float frameTime = std::chrono::duration<float, std::milli>(frameEndTime - frameStartTime_).count();

    statistics_.totalFrameTime = frameTime;
    accumulatedFrameTime_ += frameTime;
    frameCount_++;

    // 1秒ごとにFPSを更新
    if (accumulatedFrameTime_ >= 1000.0f) {
        statistics_.fps = frameCount_;
        frameCount_ = 0;
        accumulatedFrameTime_ = 0.0f;

        // GPU時間を集計
        float totalGPUTime = 0.0f;
        for (const auto& scope : scopes_) {
            totalGPUTime += scope.gpuTime;
        }
        statistics_.gpuTime = totalGPUTime;
        statistics_.cpuTime = frameTime - totalGPUTime;

        char debugStr[512];
        sprintf_s(debugStr, "PerformanceProfiler: Frame %.2fms (CPU: %.2fms, GPU: %.2fms) FPS: %u\n",
            statistics_.totalFrameTime,
            statistics_.cpuTime,
            statistics_.gpuTime,
            statistics_.fps
        );
        OutputDebugStringA(debugStr);
    }
}

void PerformanceProfiler::BeginCPUScope(const std::string& name) {
    cpuScopeStarts_[name] = std::chrono::high_resolution_clock::now();
}

void PerformanceProfiler::EndCPUScope(const std::string& name) {
    auto it = cpuScopeStarts_.find(name);
    if (it == cpuScopeStarts_.end()) {
        return;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float, std::milli>(endTime - it->second).count();

    ProfileScope scope;
    scope.name = name;
    scope.cpuTime = elapsed;
    scopes_.push_back(scope);

    cpuScopeStarts_.erase(it);
}

void PerformanceProfiler::BeginGPUScope(ID3D12GraphicsCommandList* cmdList, const std::string& name) {
    if (!cmdList || queryIndex_ >= kMaxQueries - 1) {
        return;
    }

    // タイムスタンプクエリを記録
    cmdList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex_);

    ProfileScope scope;
    scope.name = name;
    scope.gpuStartTimestamp = queryIndex_;
    scopes_.push_back(scope);

    queryIndex_++;
}

void PerformanceProfiler::EndGPUScope(ID3D12GraphicsCommandList* cmdList, const std::string& name) {
    if (!cmdList || queryIndex_ >= kMaxQueries) {
        return;
    }

    // 終了タイムスタンプ
    cmdList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIndex_);

    // スコープを更新
    auto it = std::find_if(scopes_.begin(), scopes_.end(),
        [&name](const ProfileScope& scope) { return scope.name == name; });

    if (it != scopes_.end()) {
        it->gpuEndTimestamp = queryIndex_;

        // クエリ結果を解決
        cmdList->ResolveQueryData(
            queryHeap_.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            static_cast<UINT>(it->gpuStartTimestamp),
            2,
            queryResultBuffer_.Get(),
            it->gpuStartTimestamp * sizeof(uint64_t)
        );

        // GPU時間を計算(後で読み戻す)
        // TODO: 実際にはフェンス後にReadbackして計算
        it->gpuTime = 0.0f; // Placeholder
    }

    queryIndex_++;
}

void PerformanceProfiler::ReportCullingEfficiency(uint32_t totalVertices, uint32_t renderedVertices) {
    statistics_.totalVertices = totalVertices;
    statistics_.renderedVertices = renderedVertices;

    if (totalVertices > 0) {
        uint32_t culledVertices = totalVertices - renderedVertices;
        statistics_.cullingEfficiency = (static_cast<float>(culledVertices) / static_cast<float>(totalVertices)) * 100.0f;
    }

    char debugStr[256];
    sprintf_s(debugStr, "PerformanceProfiler: Culling efficiency: %.2f%% (%u/%u vertices culled)\n",
        statistics_.cullingEfficiency,
        totalVertices - renderedVertices,
        totalVertices
    );
    OutputDebugStringA(debugStr);
}

void PerformanceProfiler::ReportMemoryUsage(size_t vramUsage, size_t vertexMemory, size_t textureMemory) {
    statistics_.totalVRAMUsage = vramUsage;
    statistics_.vertexMemory = vertexMemory;
    statistics_.textureMemory = textureMemory;

    char debugStr[256];
    sprintf_s(debugStr, "PerformanceProfiler: VRAM usage: %.2f MB (Vertex: %.2f MB, Texture: %.2f MB)\n",
        vramUsage / (1024.0f * 1024.0f),
        vertexMemory / (1024.0f * 1024.0f),
        textureMemory / (1024.0f * 1024.0f)
    );
    OutputDebugStringA(debugStr);
}

void PerformanceProfiler::PrintReport() const {
    OutputDebugStringA("\n===== Performance Report =====\n");

    char debugStr[512];

    sprintf_s(debugStr, "Frame: %.2fms (%u FPS)\n", statistics_.totalFrameTime, statistics_.fps);
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  CPU: %.2fms\n", statistics_.cpuTime);
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  GPU: %.2fms\n", statistics_.gpuTime);
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "Memory:\n");
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  Total VRAM: %.2f MB\n", statistics_.totalVRAMUsage / (1024.0f * 1024.0f));
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  Vertex: %.2f MB\n", statistics_.vertexMemory / (1024.0f * 1024.0f));
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  Texture: %.2f MB\n", statistics_.textureMemory / (1024.0f * 1024.0f));
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "Culling:\n");
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  Total vertices: %u\n", statistics_.totalVertices);
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  Rendered: %u\n", statistics_.renderedVertices);
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  Efficiency: %.2f%%\n", statistics_.cullingEfficiency);
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "Optimizations:\n");
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  Barrier reduction: %.2f%%\n", statistics_.barrierReduction);
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  VRS gain: %.2f%%\n", statistics_.vrsGain);
    OutputDebugStringA(debugStr);

    sprintf_s(debugStr, "  Async compute gain: %.2f%%\n", statistics_.asyncComputeGain);
    OutputDebugStringA(debugStr);

    OutputDebugStringA("==============================\n\n");
}
