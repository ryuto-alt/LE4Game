#include "SrvManager.h"
#include "DirectXCommon.h"
#include <cassert>

void SrvManager::Initialize(DirectXCommon* dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;

    // ディスクリプタヒープの作成
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.NumDescriptors = kMaxSRVCount;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = dxCommon_->GetDevice()->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr));

    // ディスクリプタサイズを取得
    descriptorSize = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // ImGuiなどのシステム用に最初のいくつかのインデックスを予約
    useIndex_ = 1; // 0番は予約済みとする

    // デバッグ出力
    OutputDebugStringA("SrvManager initialized successfully\n");
}

uint32_t SrvManager::Allocate() {
    // 最大数チェック
    assert(!IsMaxCount());

    // インデックスを確保してから加算
    uint32_t index = useIndex_;
    useIndex_++;

    // デバッグ出力
    OutputDebugStringA(("SrvManager: Allocated index " + std::to_string(index) + "\n").c_str());

    // 確保したインデックスを返す
    return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}

void SrvManager::CreateSRVForTexture2D(uint32_t srvIndex, Microsoft::WRL::ComPtr<ID3D12Resource> pResource, DXGI_FORMAT format, UINT mipLevels) {
    // nullptrチェック
    if (pResource == nullptr) {
        OutputDebugStringA("WARNING: Trying to create SRV for nullptr resource\n");
        return;
    }

    // SRVの設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = mipLevels;

    // SRVの作成
    dxCommon_->GetDevice()->CreateShaderResourceView(
        pResource.Get(),
        &srvDesc,
        GetCPUDescriptorHandle(srvIndex)
    );

    // デバッグ出力
    OutputDebugStringA(("SrvManager: Created SRV for Texture2D at index " + std::to_string(srvIndex) + "\n").c_str());
}

void SrvManager::CreateSRVForTextureCube(uint32_t srvIndex, Microsoft::WRL::ComPtr<ID3D12Resource> pResource, DXGI_FORMAT format, UINT mipLevels) {
    // nullptrチェック
    if (pResource == nullptr) {
        OutputDebugStringA("WARNING: Trying to create SRV for nullptr resource\n");
        return;
    }

    // SRVの設定（Cubemap用）
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = mipLevels;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

    // SRVの作成
    dxCommon_->GetDevice()->CreateShaderResourceView(
        pResource.Get(),
        &srvDesc,
        GetCPUDescriptorHandle(srvIndex)
    );

    // デバッグ出力
    OutputDebugStringA(("SrvManager: Created SRV for TextureCube at index " + std::to_string(srvIndex) + "\n").c_str());
}

void SrvManager::CreateSRVForStructuredBuffer(uint32_t srvIndex, Microsoft::WRL::ComPtr<ID3D12Resource> pResource, UINT numElements, UINT structureByteStride) {
    // nullptrチェック
    if (pResource == nullptr) {
        OutputDebugStringA("WARNING: Trying to create SRV for nullptr resource\n");
        return;
    }

    // SRVの設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    // SRVの作成
    dxCommon_->GetDevice()->CreateShaderResourceView(
        pResource.Get(),
        &srvDesc,
        GetCPUDescriptorHandle(srvIndex)
    );

    // デバッグ出力
    OutputDebugStringA(("SrvManager: Created SRV for StructuredBuffer at index " + std::to_string(srvIndex) + "\n").c_str());
}

void SrvManager::PreDraw() {
    // nullptrチェック
    if (!descriptorHeap) {
        OutputDebugStringA("ERROR: SrvManager::PreDraw called with null descriptorHeap\n");
        return;
    }

    // 描画用のDescriptorHeapの設定
    ID3D12DescriptorHeap* heaps[] = { descriptorHeap.Get() };
    dxCommon_->GetCommandList()->SetDescriptorHeaps(1, heaps);

    // デバッグ出力 (頻繁に呼ばれるので無効化)
    // OutputDebugStringA("SrvManager: Set descriptor heap for drawing\n");
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex) {
    // 基本的な安全性チェック
    if (!dxCommon_) {
        OutputDebugStringA("ERROR: SrvManager::SetGraphicsRootDescriptorTable - dxCommon_ is null\n");
        return;
    }

    auto commandList = dxCommon_->GetCommandList();
    if (!commandList) {
        OutputDebugStringA("ERROR: SrvManager::SetGraphicsRootDescriptorTable - CommandList is null\n");
        return;
    }

    if (!descriptorHeap) {
        OutputDebugStringA("ERROR: SrvManager::SetGraphicsRootDescriptorTable - DescriptorHeap is null\n");
        return;
    }

    // インデックスの範囲チェック
    if (srvIndex >= kMaxSRVCount) {
        OutputDebugStringA(("ERROR: SrvManager::SetGraphicsRootDescriptorTable called with invalid index: " + std::to_string(srvIndex) + "\n").c_str());
        return;
    }

    // 無効なSRVインデックスチェック
    if (srvIndex == UINT32_MAX) {
        OutputDebugStringA("ERROR: SrvManager::SetGraphicsRootDescriptorTable called with UINT32_MAX (invalid index)\n");
        return;
    }

    // SRVのGPUハンドルを取得
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = GetGPUDescriptorHandle(srvIndex);

    // ルートパラメータにSRVをセット
    commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, handleGPU);

    // デバッグ出力 (頻繁に呼ばれるので無効化)
    // OutputDebugStringA(("SrvManager: Set SRV index " + std::to_string(srvIndex) + " to root parameter " + std::to_string(rootParameterIndex) + "\n").c_str());
}

bool SrvManager::IsMaxCount() {
    return useIndex_ >= kMaxSRVCount;
}