#include "TransitionEffect.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "../Audio/AudioManager.h"
#include <cassert>

TransitionEffect::~TransitionEffect() {
    Finalize();
}

void TransitionEffect::Finalize() {
    // トランジション音を停止
    if (isTransitioning_ && audioManager_) {
        StopTransition();
    }

    // Unmapリソース
    if (transitionParamsResource_ && transitionParamsData_) {
        transitionParamsResource_->Unmap(0, nullptr);
        transitionParamsData_ = nullptr;
    }

    // ComPtrは自動的に解放されるが、明示的にリセット
    transitionParamsResource_.Reset();
    renderTargetResource_.Reset();
    rtvDescriptorHeap_.Reset();
    rootSignature_.Reset();
    pipelineState_.Reset();

    // SRV割り当てフラグをリセット
    srvAllocated_ = false;
    srvIndex_ = 0;
}

void TransitionEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateRenderTarget();
    CreatePipeline();

    // Constant Buffer作成
    transitionParamsResource_ = dxCommon_->CreateBufferResource(sizeof(TransitionParams));
    transitionParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&transitionParamsData_));

    // デフォルトパラメータ
    currentParams_.progress = 0.0f;
    currentParams_.aspectRatio = static_cast<float>(dxCommon_->GetCurrentWindowWidth()) /
                                  static_cast<float>(dxCommon_->GetCurrentWindowHeight());

    *transitionParamsData_ = currentParams_;

    // オーディオマネージャーの取得とノイズ音の読み込み
    audioManager_ = AudioManager::GetInstance();
    if (audioManager_) {
        audioManager_->LoadMP3(noiseAudioName_, "Resources/Audio/noize.mp3");
        audioManager_->SetVolume(noiseAudioName_, 0.5f); // デフォルト音量を設定
    }
}

void TransitionEffect::CreateRenderTarget() {
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
    resourceDesc.Width = dxCommon_->GetCurrentWindowWidth();
    resourceDesc.Height = dxCommon_->GetCurrentWindowHeight();
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;

    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
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

void TransitionEffect::CreatePipeline() {
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
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Transition.PS.hlsl", L"ps_6_0");

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

void TransitionEffect::PreDraw() {
    auto commandList = dxCommon_->GetCommandList();

    // レンダーターゲットをクリア
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);

    // Depth Stencilも取得して設定
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);

    // レンダーターゲットを設定（Depth Stencilも指定）
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle);

    // ビューポートとシザー矩形を設定
    UINT width = dxCommon_->GetCurrentWindowWidth();
    UINT height = dxCommon_->GetCurrentWindowHeight();

    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<FLOAT>(width);
    viewport.Height = static_cast<FLOAT>(height);
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = width;
    scissorRect.bottom = height;

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
}

void TransitionEffect::PostDraw() {
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

    // バックバッファに対してトランジションエフェクトを適用
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());

    // パラメータ設定
    commandList->SetGraphicsRootConstantBufferView(0, transitionParamsResource_->GetGPUVirtualAddress());
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

void TransitionEffect::SetProgress(float progress) {
    currentParams_.progress = progress;

    if (transitionParamsData_) {
        *transitionParamsData_ = currentParams_;
    }
}

void TransitionEffect::ResizeRenderTarget() {
    // 既存のレンダーターゲットをリセット
    renderTargetResource_.Reset();
    rtvDescriptorHeap_.Reset();

    // 新しいサイズでレンダーターゲットを再作成
    CreateRenderTarget();

    // アスペクト比を更新
    currentParams_.aspectRatio = static_cast<float>(dxCommon_->GetCurrentWindowWidth()) /
                                  static_cast<float>(dxCommon_->GetCurrentWindowHeight());
    if (transitionParamsData_) {
        *transitionParamsData_ = currentParams_;
    }
}

void TransitionEffect::StartTransition() {
    if (audioManager_ && !isTransitioning_) {
        audioManager_->Play(noiseAudioName_, true); // ループ再生
        isTransitioning_ = true;
    }
}

void TransitionEffect::StopTransition() {
    if (audioManager_ && isTransitioning_) {
        audioManager_->Stop(noiseAudioName_);
        isTransitioning_ = false;
    }
}

void TransitionEffect::SetNoiseVolume(float volume) {
    if (audioManager_) {
        audioManager_->SetVolume(noiseAudioName_, volume);
    }
}
