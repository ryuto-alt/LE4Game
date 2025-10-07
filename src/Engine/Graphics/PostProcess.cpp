#include "PostProcess.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <cassert>

PostProcess::~PostProcess() {
    OutputDebugStringA("PostProcess::~PostProcess() called\n");
    Finalize();
    OutputDebugStringA("PostProcess::~PostProcess() completed\n");
}

void PostProcess::Finalize() {
    OutputDebugStringA("PostProcess::Finalize() called\n");
    // Unmapリソース
    if (horrorParamsResource_ && horrorParamsData_) {
        horrorParamsResource_->Unmap(0, nullptr);
        horrorParamsData_ = nullptr;
    }

    // ComPtrは自動的に解放されるが、明示的にリセット
    horrorParamsResource_.Reset();
    renderTargetResource_.Reset();
    rtvDescriptorHeap_.Reset();
    rootSignature_.Reset();
    pipelineState_.Reset();

    // SRV割り当てフラグをリセット
    srvAllocated_ = false;
    srvIndex_ = 0;
    OutputDebugStringA("PostProcess::Finalize() completed\n");
}

void PostProcess::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateRenderTarget();
    CreatePipeline();

    // Constant Buffer作成
    horrorParamsResource_ = dxCommon_->CreateBufferResource(sizeof(HorrorParams));
    horrorParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&horrorParamsData_));

    // デフォルトパラメータ
    currentParams_.time = 0.0f;
    currentParams_.noiseIntensity = 0.3f;
    currentParams_.distortionAmount = 0.5f;
    currentParams_.bloodAmount = 0.4f;
    currentParams_.vignetteIntensity = 0.0f;
    currentParams_.fisheyeStrength = 0.0f;  // デフォルトはオフ

    *horrorParamsData_ = currentParams_;
}

void PostProcess::CreateRenderTarget() {
    HRESULT hr;

    // RTV用のDescriptorHeap作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    hr = dxCommon_->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap_));
    assert(SUCCEEDED(hr));

    // レンダーターゲット用のテクスチャリソースを作成
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = 1280;
    resourceDesc.Height = 720;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = 0.1f;
    clearValue.Color[1] = 0.25f;
    clearValue.Color[2] = 0.5f;
    clearValue.Color[3] = 1.0f;

    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, // 初期状態をRENDER_TARGETに変更
        &clearValue,
        IID_PPV_ARGS(&renderTargetResource_)
    );
    assert(SUCCEEDED(hr));

    // RTVを作成
    rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    dxCommon_->GetDevice()->CreateRenderTargetView(renderTargetResource_.Get(), nullptr, rtvHandle_);

    // SRVを作成（SrvManagerを使用）
    if (!srvAllocated_) {
        srvIndex_ = srvManager_->Allocate();
        srvAllocated_ = true;
    }
    srvManager_->CreateSRVForTexture2D(srvIndex_, renderTargetResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
    srvGPUHandle_ = srvManager_->GetGPUDescriptorHandle(srvIndex_);
}

void PostProcess::CreatePipeline() {
    HRESULT hr;

    // RootSignature作成
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    // シェーダーコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Fullscreen.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Horror.PS.hlsl", L"ps_6_0");

    // Pipeline State作成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();
    pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

    pipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // 入力レイアウト（不要だがNULLは許可されない場合があるので空を設定）
    pipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineStateDesc.InputLayout.NumElements = 0;

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void PostProcess::PreDraw() {
    auto commandList = dxCommon_->GetCommandList();

    // リソースバリア：PIXEL_SHADER_RESOURCEからRENDER_TARGETへ（2フレーム目以降用）
    // 初回はすでにRENDER_TARGETなので、状態を追跡する必要がある
    // 簡単のため、常にバリアを張る（初回は何もしない状態から始まるので問題ない）

    // レンダーターゲットをクリア
    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);

    // Depth Stencilも取得して設定
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);

    // レンダーターゲットを設定（Depth Stencilも指定）
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle);
}

void PostProcess::PostDraw() {
    auto commandList = dxCommon_->GetCommandList();

    // リソースバリア：RENDER_TARGETからPIXEL_SHADER_RESOURCEへ
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = renderTargetResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    // バックバッファのRTVとDSVを再設定
    UINT backBufferIndex = dxCommon_->GetBackBufferIndex();
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = dxCommon_->GetRTVCPUDescriptorHandle(backBufferIndex);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);
    commandList->OMSetRenderTargets(1, &backBufferRTV, false, &dsvHandle);

    // バックバッファに対してホラーエフェクトを適用
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());

    // パラメータ設定
    commandList->SetGraphicsRootConstantBufferView(0, horrorParamsResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(1, srvGPUHandle_);

    // フルスクリーン三角形を描画
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);

    // リソースバリア：次のフレームのためにRENDER_TARGETに戻す
    D3D12_RESOURCE_BARRIER barrier2{};
    barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier2.Transition.pResource = renderTargetResource_.Get();
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier2);
}

void PostProcess::SetHorrorParams(float time, float noise, float distortion, float blood, float vignette) {
    currentParams_.time = time;
    currentParams_.noiseIntensity = noise;
    currentParams_.distortionAmount = distortion;
    currentParams_.bloodAmount = blood;
    currentParams_.vignetteIntensity = vignette;

    if (horrorParamsData_) {
        *horrorParamsData_ = currentParams_;
    }
}

void PostProcess::SetFisheyeStrength(float strength) {
    currentParams_.fisheyeStrength = strength;

    if (horrorParamsData_) {
        *horrorParamsData_ = currentParams_;
    }
}
