#include "GPUDrivenCulling.h"
#include "DirectXCommon.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

GPUDrivenCulling::GPUDrivenCulling() = default;

GPUDrivenCulling::~GPUDrivenCulling() {
    // リソース解放
    objects_.clear();
    drawArgs_.clear();
}

void GPUDrivenCulling::Initialize(ID3D12Device* device, uint32_t maxDrawCalls) {
    assert(device);
    device_ = device;
    maxDrawCalls_ = maxDrawCalls;

    // バッファサイズ計算
    size_t objectDataSize = sizeof(ObjectData) * maxDrawCalls;
    size_t drawCommandSize = sizeof(DrawCommand) * maxDrawCalls;
    size_t constantsSize = sizeof(CullingConstants);

    // オブジェクトデータバッファ(SRV)
    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = objectDataSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device_->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&objectDataBuffer_)
    );
    assert(SUCCEEDED(hr));

    // ドローコマンドバッファ(UAV)
    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    bufferDesc.Width = drawCommandSize;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = device_->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&drawCommandBuffer_)
    );
    assert(SUCCEEDED(hr));

    // カリング定数バッファ
    bufferDesc.Width = constantsSize;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = device_->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&cullingConstantsBuffer_)
    );
    assert(SUCCEEDED(hr));

    // カウントバッファ(可視オブジェクト数)
    bufferDesc.Width = sizeof(uint32_t);
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = device_->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&countBuffer_)
    );
    assert(SUCCEEDED(hr));

    // CommandSignature作成(ExecuteIndirect用)
    D3D12_INDIRECT_ARGUMENT_DESC argumentDesc = {};
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

    D3D12_COMMAND_SIGNATURE_DESC signatureDesc = {};
    signatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
    signatureDesc.NumArgumentDescs = 1;
    signatureDesc.pArgumentDescs = &argumentDesc;
    signatureDesc.NodeMask = 0;

    hr = device_->CreateCommandSignature(
        &signatureDesc,
        nullptr,
        IID_PPV_ARGS(&commandSignature_)
    );
    assert(SUCCEEDED(hr));

    OutputDebugStringA("GPUDrivenCulling: Initialized successfully\n");
    char debugStr[256];
    sprintf_s(debugStr, "GPUDrivenCulling: Max draw calls: %u\n", maxDrawCalls_);
    OutputDebugStringA(debugStr);

    // TODO: コンピュートシェーダーの作成
    // CreateCullingShader();
}

void GPUDrivenCulling::RegisterObject(const ObjectData& object, const D3D12_DRAW_ARGUMENTS& drawArgs) {
    if (objects_.size() >= maxDrawCalls_) {
        OutputDebugStringA("WARNING: GPUDrivenCulling: Max draw calls reached!\n");
        return;
    }

    objects_.push_back(object);
    drawArgs_.push_back(drawArgs);

    statistics_.totalObjects++;
}

void GPUDrivenCulling::ExecuteCulling(
    ID3D12GraphicsCommandList* cmdList,
    const Matrix4x4& viewProjection,
    const Vector3& cameraPosition,
    float nearPlane,
    float farPlane
) {
    assert(cmdList);

    if (objects_.empty()) {
        return;
    }

    // 定数バッファ更新
    CullingConstants* constants = nullptr;
    cullingConstantsBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constants));

    constants->viewProjection = viewProjection;
    constants->cameraPosition = cameraPosition;
    constants->nearPlane = nearPlane;
    constants->farPlane = farPlane;
    constants->totalDrawCalls = static_cast<uint32_t>(objects_.size());

    CalculateFrustumPlanes(viewProjection, constants->frustumPlanes);

    cullingConstantsBuffer_->Unmap(0, nullptr);

    // オブジェクトデータ更新
    ObjectData* objectData = nullptr;
    objectDataBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&objectData));
    std::memcpy(objectData, objects_.data(), sizeof(ObjectData) * objects_.size());
    objectDataBuffer_->Unmap(0, nullptr);

    // TODO: コンピュートシェーダーでカリング実行
    // cmdList->SetPipelineState(cullingPSO_.Get());
    // cmdList->SetComputeRootSignature(cullingRootSignature_.Get());
    // cmdList->Dispatch(...);

    OutputDebugStringA("GPUDrivenCulling: Culling executed\n");
}

void GPUDrivenCulling::ExecuteIndirectDraw(ID3D12GraphicsCommandList* cmdList, ID3D12PipelineState* pso) {
    assert(cmdList);
    assert(pso);

    if (objects_.empty()) {
        return;
    }

    // ExecuteIndirect でカリング済みドローコールを実行
    cmdList->ExecuteIndirect(
        commandSignature_.Get(),
        static_cast<UINT>(objects_.size()),
        drawCommandBuffer_.Get(),
        0,
        countBuffer_.Get(),
        0
    );

    char debugStr[256];
    sprintf_s(debugStr, "GPUDrivenCulling: ExecuteIndirect with %zu potential draw calls\n", objects_.size());
    OutputDebugStringA(debugStr);
}

void GPUDrivenCulling::Reset() {
    objects_.clear();
    drawArgs_.clear();

    // 統計情報をリセット
    statistics_ = Statistics{};
}

void GPUDrivenCulling::CalculateFrustumPlanes(const Matrix4x4& viewProjection, Vector4 planes[6]) {
    // ビュープロジェクション行列からフラスタム平面を抽出
    // Left plane
    planes[0].x = viewProjection.m[0][3] + viewProjection.m[0][0];
    planes[0].y = viewProjection.m[1][3] + viewProjection.m[1][0];
    planes[0].z = viewProjection.m[2][3] + viewProjection.m[2][0];
    planes[0].w = viewProjection.m[3][3] + viewProjection.m[3][0];

    // Right plane
    planes[1].x = viewProjection.m[0][3] - viewProjection.m[0][0];
    planes[1].y = viewProjection.m[1][3] - viewProjection.m[1][0];
    planes[1].z = viewProjection.m[2][3] - viewProjection.m[2][0];
    planes[1].w = viewProjection.m[3][3] - viewProjection.m[3][0];

    // Bottom plane
    planes[2].x = viewProjection.m[0][3] + viewProjection.m[0][1];
    planes[2].y = viewProjection.m[1][3] + viewProjection.m[1][1];
    planes[2].z = viewProjection.m[2][3] + viewProjection.m[2][1];
    planes[2].w = viewProjection.m[3][3] + viewProjection.m[3][1];

    // Top plane
    planes[3].x = viewProjection.m[0][3] - viewProjection.m[0][1];
    planes[3].y = viewProjection.m[1][3] - viewProjection.m[1][1];
    planes[3].z = viewProjection.m[2][3] - viewProjection.m[2][1];
    planes[3].w = viewProjection.m[3][3] - viewProjection.m[3][1];

    // Near plane
    planes[4].x = viewProjection.m[0][2];
    planes[4].y = viewProjection.m[1][2];
    planes[4].z = viewProjection.m[2][2];
    planes[4].w = viewProjection.m[3][2];

    // Far plane
    planes[5].x = viewProjection.m[0][3] - viewProjection.m[0][2];
    planes[5].y = viewProjection.m[1][3] - viewProjection.m[1][2];
    planes[5].z = viewProjection.m[2][3] - viewProjection.m[2][2];
    planes[5].w = viewProjection.m[3][3] - viewProjection.m[3][2];

    // 各平面を正規化
    for (int i = 0; i < 6; ++i) {
        float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
        if (length > 0.0001f) {
            planes[i].x /= length;
            planes[i].y /= length;
            planes[i].z /= length;
            planes[i].w /= length;
        }
    }
}

void GPUDrivenCulling::CreateCullingShader() {
    // TODO: HLSLコンピュートシェーダーをコンパイル
    // フラスタムカリング用のシェーダーを作成
    OutputDebugStringA("GPUDrivenCulling: Culling shader creation (TODO)\n");
}
