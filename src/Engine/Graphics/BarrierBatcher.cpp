#include "BarrierBatcher.h"
#include <cassert>
#include <algorithm>
#include <cstdio>

BarrierBatcher::BarrierBatcher() {
    // 事前にメモリを確保してreallocationを避ける
    pendingBarriers_.reserve(kMaxBatchSize);
}

BarrierBatcher::~BarrierBatcher() = default;

void BarrierBatcher::AddTransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter,
    UINT subresource
) {
    assert(resource);

    // 冗長チェック
    if (IsRedundant(resource, stateBefore, stateAfter, subresource)) {
        statistics_.redundantBarriersRemoved++;
        return;
    }

    // バリアを追加
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = subresource;

    pendingBarriers_.push_back(barrier);
    statistics_.totalBarriers++;

    // リソース状態を更新
    UpdateResourceState(resource, stateAfter, subresource);
}

void BarrierBatcher::AddUAVBarrier(ID3D12Resource* resource) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = resource;

    pendingBarriers_.push_back(barrier);
    statistics_.totalBarriers++;
}

void BarrierBatcher::BeginSplitBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter,
    UINT subresource
) {
    assert(resource);

    // スプリットバリアの開始
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY; // 開始フラグ
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = subresource;

    pendingBarriers_.push_back(barrier);
    statistics_.totalBarriers++;
}

void BarrierBatcher::EndSplitBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter,
    UINT subresource
) {
    assert(resource);

    // スプリットバリアの終了
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY; // 終了フラグ
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = subresource;

    pendingBarriers_.push_back(barrier);
    statistics_.totalBarriers++;

    // リソース状態を更新
    UpdateResourceState(resource, stateAfter, subresource);
}

void BarrierBatcher::Flush(ID3D12GraphicsCommandList* cmdList) {
    if (pendingBarriers_.empty()) {
        return;
    }

    assert(cmdList);

    // バリアをバッチ送信
    cmdList->ResourceBarrier(
        static_cast<UINT>(pendingBarriers_.size()),
        pendingBarriers_.data()
    );

    statistics_.batchCount++;

    // クリア
    pendingBarriers_.clear();
}

void BarrierBatcher::Reset() {
    pendingBarriers_.clear();
    resourceStates_.clear();

    // 統計リセット (ログ出力は削除してパフォーマンス向上)
    statistics_ = Statistics{};
}

bool BarrierBatcher::IsRedundant(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter, UINT subresource) {
    auto it = resourceStates_.find(resource);
    if (it == resourceStates_.end()) {
        return false; // 初回は冗長ではない
    }

    const ResourceState& state = it->second;

    // 同じサブリソースで、すでに目的の状態にある場合は冗長
    if (state.subresource == subresource && state.currentState == stateAfter) {
        return true;
    }

    // stateBefore が現在の状態と一致しない場合も冗長(不正なバリア)
    if (state.subresource == subresource && state.currentState != stateBefore) {
        char debugStr[256];
        sprintf_s(debugStr, "WARNING: BarrierBatcher: State mismatch! Expected %u, got %u\n",
            state.currentState, stateBefore);
        OutputDebugStringA(debugStr);
    }

    return false;
}

void BarrierBatcher::UpdateResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES newState, UINT subresource) {
    ResourceState state;
    state.currentState = newState;
    state.subresource = subresource;

    resourceStates_[resource] = state;
}
