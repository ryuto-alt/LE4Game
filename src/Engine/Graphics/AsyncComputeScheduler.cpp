#include "AsyncComputeScheduler.h"
#include <cassert>
#include <algorithm>
#include <cstdio>

AsyncComputeScheduler::AsyncComputeScheduler() = default;

AsyncComputeScheduler::~AsyncComputeScheduler() {
    WaitForCompletion();

    if (computeFenceEvent_) {
        CloseHandle(computeFenceEvent_);
        computeFenceEvent_ = nullptr;
    }

    computeCmdList_.Reset();
    computeAllocator_.Reset();
    computeQueue_.Reset();
    computeFence_.Reset();
}

void AsyncComputeScheduler::Initialize(ID3D12Device* device) {
    assert(device);
    device_ = device;

    // コンピュートキュー作成
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE; // コンピュート専用
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    HRESULT hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&computeQueue_));
    assert(SUCCEEDED(hr));

    // コマンドアロケータ作成
    hr = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        IID_PPV_ARGS(&computeAllocator_)
    );
    assert(SUCCEEDED(hr));

    // コマンドリスト作成
    hr = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        computeAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&computeCmdList_)
    );
    assert(SUCCEEDED(hr));

    computeCmdList_->Close();

    // フェンス作成
    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&computeFence_));
    assert(SUCCEEDED(hr));

    computeFenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(computeFenceEvent_);

    OutputDebugStringA("AsyncComputeScheduler: Initialized\n");
}

void AsyncComputeScheduler::AddTask(const ComputeTask& task) {
    tasks_.push_back(task);
    statistics_.totalTasks++;
}

void AsyncComputeScheduler::ExecuteAsync(ID3D12CommandQueue* graphicsQueue) {
    if (tasks_.empty()) {
        return;
    }

    // 優先度でソート(高優先度から実行)
    std::sort(tasks_.begin(), tasks_.end(),
        [](const ComputeTask& a, const ComputeTask& b) {
            return a.priority > b.priority;
        });

    // コマンドリストをリセット
    computeAllocator_->Reset();
    computeCmdList_->Reset(computeAllocator_.Get(), nullptr);

    // タスクを実行
    for (const auto& task : tasks_) {
        if (task.taskFunc) {
            task.taskFunc(computeCmdList_.Get());
        }
    }

    computeCmdList_->Close();

    // コンピュートキューに投入(グラフィックスキューと並行実行)
    ID3D12CommandList* cmdLists[] = { computeCmdList_.Get() };
    computeQueue_->ExecuteCommandLists(1, cmdLists);

    // フェンスシグナル
    computeFenceValue_++;
    computeQueue_->Signal(computeFence_.Get(), computeFenceValue_);

    char debugStr[256];
    sprintf_s(debugStr, "AsyncComputeScheduler: Executing %zu tasks asynchronously\n", tasks_.size());
    OutputDebugStringA(debugStr);
}

void AsyncComputeScheduler::WaitForCompletion() {
    if (computeFence_->GetCompletedValue() < computeFenceValue_) {
        computeFence_->SetEventOnCompletion(computeFenceValue_, computeFenceEvent_);
        WaitForSingleObject(computeFenceEvent_, INFINITE);
    }

    statistics_.completedTasks = statistics_.totalTasks;

    OutputDebugStringA("AsyncComputeScheduler: All tasks completed\n");
}

void AsyncComputeScheduler::Reset() {
    tasks_.clear();

    char debugStr[256];
    sprintf_s(debugStr, "AsyncComputeScheduler: Frame stats - Total: %u, Completed: %u, Efficiency: %.2f%%\n",
        statistics_.totalTasks,
        statistics_.completedTasks,
        statistics_.parallelEfficiency
    );
    OutputDebugStringA(debugStr);

    statistics_ = Statistics{};
}
