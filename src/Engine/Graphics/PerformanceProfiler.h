#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <cstdint>

// パフォーマンスプロファイラー
// フレーム全体、GPU時間、カリング効率などを測定
class PerformanceProfiler {
public:
    // プロファイルスコープ
    struct ProfileScope {
        std::string name;
        float cpuTime = 0.0f; // ms
        float gpuTime = 0.0f; // ms
        uint64_t gpuStartTimestamp = 0;
        uint64_t gpuEndTimestamp = 0;
    };

    // 全体統計
    struct FrameStatistics {
        float totalFrameTime = 0.0f; // ms
        float cpuTime = 0.0f; // ms
        float gpuTime = 0.0f; // ms
        uint32_t fps = 0;

        // メモリ統計
        size_t totalVRAMUsage = 0; // bytes
        size_t vertexMemory = 0; // bytes
        size_t textureMemory = 0; // bytes

        // カリング統計
        uint32_t totalVertices = 0;
        uint32_t renderedVertices = 0;
        float cullingEfficiency = 0.0f; // %

        // 最適化効果
        float barrierReduction = 0.0f; // %
        float vrsGain = 0.0f; // %
        float asyncComputeGain = 0.0f; // %
    };

    PerformanceProfiler();
    ~PerformanceProfiler();

    // 初期化
    void Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue);

    // フレーム開始
    void BeginFrame();

    // フレーム終了
    void EndFrame();

    // CPU時間スコープ開始
    void BeginCPUScope(const std::string& name);

    // CPU時間スコープ終了
    void EndCPUScope(const std::string& name);

    // GPU時間スコープ開始
    void BeginGPUScope(ID3D12GraphicsCommandList* cmdList, const std::string& name);

    // GPU時間スコープ終了
    void EndGPUScope(ID3D12GraphicsCommandList* cmdList, const std::string& name);

    // 統計情報を取得
    const FrameStatistics& GetStatistics() const { return statistics_; }

    // カリング効率を報告
    void ReportCullingEfficiency(uint32_t totalVertices, uint32_t renderedVertices);

    // メモリ使用量を報告
    void ReportMemoryUsage(size_t vramUsage, size_t vertexMemory, size_t textureMemory);

    // レポート出力(ImGuiまたはコンソール)
    void PrintReport() const;

private:
    ID3D12Device* device_ = nullptr;
    ID3D12CommandQueue* commandQueue_ = nullptr;

    // GPUタイムスタンプクエリ
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> queryResultBuffer_;
    uint32_t queryIndex_ = 0;
    static constexpr uint32_t kMaxQueries = 128;

    // CPU時間測定
    std::chrono::high_resolution_clock::time_point frameStartTime_;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> cpuScopeStarts_;

    // プロファイルスコープ
    std::vector<ProfileScope> scopes_;

    // フレーム統計
    FrameStatistics statistics_;

    // フレームカウンター
    uint32_t frameCount_ = 0;
    float accumulatedFrameTime_ = 0.0f;

    // GPU周波数
    uint64_t gpuFrequency_ = 0;
};
