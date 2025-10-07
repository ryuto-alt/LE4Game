#include "MultiThreadCommandList.h"
#include <cassert>
#include <algorithm>
#include <cstdio>

MultiThreadCommandList::MultiThreadCommandList() = default;

MultiThreadCommandList::~MultiThreadCommandList() {
    // スレッドを終了
    shouldTerminate_ = true;
    queueCV_.notify_all();

    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void MultiThreadCommandList::Initialize(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) {
    assert(device);
    device_ = device;
    commandListType_ = type;

    // ハードウェアスレッド数を取得(最大8スレッド)
    uint32_t hardwareThreads = std::thread::hardware_concurrency();
    workerCount_ = (hardwareThreads < kMaxWorkerThreads) ? hardwareThreads : kMaxWorkerThreads;

    // デバッグ出力
    char debugStr[256];
    sprintf_s(debugStr, "MultiThreadCommandList: Initializing with %u worker threads\n", workerCount_);
    OutputDebugStringA(debugStr);

    // 各スレッド用のリソースを作成
    commandAllocators_.resize(workerCount_);
    commandLists_.resize(workerCount_);

    for (uint32_t i = 0; i < workerCount_; ++i) {
        // コマンドアロケータ作成
        HRESULT hr = device_->CreateCommandAllocator(
            commandListType_,
            IID_PPV_ARGS(&commandAllocators_[i])
        );
        assert(SUCCEEDED(hr));

        // コマンドリスト作成
        hr = device_->CreateCommandList(
            0,
            commandListType_,
            commandAllocators_[i].Get(),
            nullptr,
            IID_PPV_ARGS(&commandLists_[i])
        );
        assert(SUCCEEDED(hr));

        // 初期状態でClose
        commandLists_[i]->Close();
    }

    // ワーカースレッドを起動
    workerThreads_.reserve(workerCount_);
    for (uint32_t i = 0; i < workerCount_; ++i) {
        workerThreads_.emplace_back(&MultiThreadCommandList::WorkerThread, this, i);
    }

    OutputDebugStringA("MultiThreadCommandList: Initialization complete\n");
}

void MultiThreadCommandList::AddTask(const RecordingTask& task) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    taskQueue_.push(task);
}

std::vector<ID3D12CommandList*> MultiThreadCommandList::ExecuteAndGetCommandLists() {
    // すべてのワーカーにタスク処理を開始させる
    activeWorkers_ = workerCount_;
    queueCV_.notify_all();

    // すべてのワーカーが完了するまで待機
    {
        std::unique_lock<std::mutex> lock(workersFinishedMutex_);
        workersFinishedCV_.wait(lock, [this]() { return activeWorkers_ == 0; });
    }

    // コマンドリストを収集
    std::vector<ID3D12CommandList*> commandListsToExecute;
    commandListsToExecute.reserve(workerCount_);

    for (uint32_t i = 0; i < workerCount_; ++i) {
        commandListsToExecute.push_back(commandLists_[i].Get());
    }

    return commandListsToExecute;
}

void MultiThreadCommandList::Reset() {
    // 次フレーム用にリセット
    for (uint32_t i = 0; i < workerCount_; ++i) {
        HRESULT hr = commandAllocators_[i]->Reset();
        assert(SUCCEEDED(hr));

        hr = commandLists_[i]->Reset(commandAllocators_[i].Get(), nullptr);
        assert(SUCCEEDED(hr));
    }

    // タスクキューをクリア
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::queue<RecordingTask> empty;
        std::swap(taskQueue_, empty);
    }
}

void MultiThreadCommandList::WorkerThread(uint32_t threadIndex) {
    char debugStr[256];
    sprintf_s(debugStr, "MultiThreadCommandList: Worker thread %u started\n", threadIndex);
    OutputDebugStringA(debugStr);

    while (!shouldTerminate_) {
        RecordingTask task;
        bool hasTask = false;

        // タスクを取得
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait(lock, [this]() {
                return !taskQueue_.empty() || shouldTerminate_;
            });

            if (!taskQueue_.empty()) {
                task = taskQueue_.front();
                taskQueue_.pop();
                hasTask = true;
            }
        }

        // タスク実行
        if (hasTask && !shouldTerminate_) {
            try {
                // コマンドリストにコマンドを記録
                task.recordFunc(commandLists_[threadIndex].Get());

                // コマンドリストをClose
                commandLists_[threadIndex]->Close();
            }
            catch (...) {
                sprintf_s(debugStr, "MultiThreadCommandList: Worker thread %u encountered an error\n", threadIndex);
                OutputDebugStringA(debugStr);
            }

            // ワーカーカウントを減らす
            uint32_t remaining = --activeWorkers_;
            if (remaining == 0) {
                workersFinishedCV_.notify_one();
            }
        }
    }

    sprintf_s(debugStr, "MultiThreadCommandList: Worker thread %u terminated\n", threadIndex);
    OutputDebugStringA(debugStr);
}
