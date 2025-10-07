#include "DepthPrepass.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <algorithm>

DepthPrepassRenderer::DepthPrepassRenderer() = default;

DepthPrepassRenderer::~DepthPrepassRenderer() {
    depthBuffer_.Reset();
    dsvHeap_.Reset();
    depthOnlyPSO_.Reset();
    hiZBuffer_.Reset();
}

void DepthPrepassRenderer::Initialize(ID3D12Device* device, uint32_t width, uint32_t height) {
    assert(device);
    device_ = device;
    width_ = width;
    height_ = height;

    // 深度バッファ作成
    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Alignment = 0;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT; // 32bit深度
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthBuffer_)
    );
    assert(SUCCEEDED(hr));

    // DSVヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_));
    assert(SUCCEEDED(hr));

    // DSV作成
    dsvHandle_ = dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    device_->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHandle_);

    // Hi-Zピラミッド用のミップレベル計算
    uint32_t maxDim = (width > height) ? width : height;
    hiZMipLevels_ = static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(maxDim)))) + 1;

    OutputDebugStringA("DepthPrepassRenderer: Initialized\n");
    char debugStr[256];
    sprintf_s(debugStr, "DepthPrepassRenderer: Depth buffer %ux%u, Hi-Z mips: %u\n",
        width, height, hiZMipLevels_);
    OutputDebugStringA(debugStr);
}

void DepthPrepassRenderer::CreateDepthOnlyPSO(
    ID3D12RootSignature* rootSignature,
    const D3D12_INPUT_LAYOUT_DESC& inputLayout
) {
    assert(rootSignature);

    // 深度専用PSO設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature;

    // シェーダー: ピクセルシェーダーなし(深度のみ)
    // TODO: 頂点シェーダーは既存のものを使用
    // psoDesc.VS = ...;
    psoDesc.PS = {}; // ピクセルシェーダーなし

    // ラスタライザー
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = 0;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // ブレンド: 無効(RTVなし)
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = 0; // 書き込みなし
    }

    // 深度ステンシル: 深度書き込み有効、Early-Z有効化
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // Early-Z最適化
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    // 入力レイアウト
    psoDesc.InputLayout = inputLayout;

    // プリミティブトポロジ
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // レンダーターゲット: なし(深度のみ)
    psoDesc.NumRenderTargets = 0;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    // サンプル
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    // TODO: シェーダーバイトコードを設定してからPSO作成
    // HRESULT hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&depthOnlyPSO_));
    // assert(SUCCEEDED(hr));

    OutputDebugStringA("DepthPrepassRenderer: Depth-only PSO created\n");
}

void DepthPrepassRenderer::ExecuteDepthPrepass(
    ID3D12GraphicsCommandList* cmdList,
    const std::function<void()>& drawCallback
) {
    assert(cmdList);
    assert(depthOnlyPSO_);

    // 深度バッファをクリア
    cmdList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 深度専用PSOを設定
    cmdList->SetPipelineState(depthOnlyPSO_.Get());

    // DSVを設定(RTVなし)
    cmdList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle_);

    // 描画コールバック実行
    if (drawCallback) {
        drawCallback();
    }

    OutputDebugStringA("DepthPrepassRenderer: Depth prepass executed\n");
}

void DepthPrepassRenderer::GenerateHiZPyramid(ID3D12GraphicsCommandList* cmdList) {
    assert(cmdList);

    if (!hiZBuffer_) {
        // Hi-Zバッファ作成(ミップマップチェーン)
        D3D12_RESOURCE_DESC hiZDesc = {};
        hiZDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        hiZDesc.Width = width_;
        hiZDesc.Height = height_;
        hiZDesc.DepthOrArraySize = 1;
        hiZDesc.MipLevels = static_cast<UINT16>(hiZMipLevels_);
        hiZDesc.Format = DXGI_FORMAT_R32_FLOAT;
        hiZDesc.SampleDesc.Count = 1;
        hiZDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        hiZDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        HRESULT hr = device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &hiZDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            IID_PPV_ARGS(&hiZBuffer_)
        );

        if (FAILED(hr)) {
            OutputDebugStringA("ERROR: DepthPrepassRenderer: Failed to create Hi-Z buffer\n");
            return;
        }
    }

    // TODO: コンピュートシェーダーでミップマップ生成
    // 各ミップレベルで最大深度を計算してオクルージョンカリングに使用

    OutputDebugStringA("DepthPrepassRenderer: Hi-Z pyramid generated\n");
}
