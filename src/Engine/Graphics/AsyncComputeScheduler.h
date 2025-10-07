#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <functional>
#include <cstdint>

// 非同期コンピュートスケジューラー
// AMD GPUで10-30%のパフォーマンス向上を目指す
class AsyncComputeScheduler {
public:
    // コンピュートタスク
    struct ComputeTask {
        std::function<void(ID3D12GraphicsCommandList*)> taskFunc;
        uint32_t priority = 0; // 優先度(高いほど優先)
    };

    AsyncComputeScheduler();
    ~AsyncComputeScheduler();

    // 初期化
    void Initialize(ID3D12Device* device);

    // コンピュートタスクを追加
    void AddTask(const ComputeTask& task);

    // グラフィックスキューと並行してコンピュートタスクを実行
    void ExecuteAsync(ID3D12CommandQueue* graphicsQueue);

    // 完了を待機
    void WaitForCompletion();

    // GPU使用率を取得(0.0-1.0)
    float GetGPUUtilization() const { return gpuUtilization_; }

    // 統計情報
    struct Statistics {
        uint32_t totalTasks = 0;
        uint32_t completedTasks = 0;
        float averageExecutionTime = 0.0f; // ms
        float parallelEfficiency = 0.0f; // %
    };

    const Statistics& GetStatistics() const { return statistics_; }

    // リセット
    void Reset();

private:
    ID3D12Device* device_ = nullptr;

    // コンピュートキュー(非同期)
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> computeQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> computeAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> computeCmdList_;

    // 同期プリミティブ
    Microsoft::WRL::ComPtr<ID3D12Fence> computeFence_;
    HANDLE computeFenceEvent_ = nullptr;
    uint64_t computeFenceValue_ = 0;

    // タスクキュー
    std::vector<ComputeTask> tasks_;

    // GPU使用率
    float gpuUtilization_ = 0.0f;

    // 統計情報
    Statistics statistics_;
};
