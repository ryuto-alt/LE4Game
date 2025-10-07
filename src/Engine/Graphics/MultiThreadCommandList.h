#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <atomic>
#include <cstdint>

// マルチスレッドコマンドリストシステム
// 大規模頂点データ(5860万頂点)を効率的にレンダリングするための並列化システム
class MultiThreadCommandList {
public:
    // ワーカースレッド数(6-8推奨)
    static constexpr uint32_t kMaxWorkerThreads = 8;

    // コマンドリスト記録タスク
    struct RecordingTask {
        std::function<void(ID3D12GraphicsCommandList*)> recordFunc;
        uint32_t threadIndex;
    };

    MultiThreadCommandList();
    ~MultiThreadCommandList();

    // 初期化
    void Initialize(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

    // タスクを追加
    void AddTask(const RecordingTask& task);

    // すべてのタスクを実行してコマンドリストを取得
    std::vector<ID3D12CommandList*> ExecuteAndGetCommandLists();

    // リセット(次フレーム用)
    void Reset();

    // ワーカースレッド数を取得
    uint32_t GetWorkerCount() const { return workerCount_; }

private:
    // ワーカースレッドのメインループ
    void WorkerThread(uint32_t threadIndex);

    // デバイス
    ID3D12Device* device_ = nullptr;

    // 各スレッド用のコマンドアロケータとリスト
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> commandAllocators_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> commandLists_;

    // ワーカースレッド
    std::vector<std::thread> workerThreads_;
    uint32_t workerCount_ = 0;

    // タスクキュー
    std::queue<RecordingTask> taskQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;

    // スレッド制御
    std::atomic<bool> shouldTerminate_{false};
    std::atomic<uint32_t> activeWorkers_{0};
    std::condition_variable workersFinishedCV_;
    std::mutex workersFinishedMutex_;

    // コマンドリストタイプ
    D3D12_COMMAND_LIST_TYPE commandListType_ = D3D12_COMMAND_LIST_TYPE_DIRECT;
};
