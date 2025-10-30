#include "LineRenderer.h"
#include <cassert>

LineRenderer::LineRenderer() {}

LineRenderer::~LineRenderer() {
    if (transformData_) {
        transformBuffer_->Unmap(0, nullptr);
        transformData_ = nullptr;
    }
}

void LineRenderer::Initialize(DirectXCommon* dxCommon, Camera* camera) {
    dxCommon_ = dxCommon;
    camera_ = camera;

    CreatePipelineState();
    CreateBuffers();
}

void LineRenderer::AddLine(const Vector3& start, const Vector3& end, const Vector4& color) {
    LineVertex v1, v2;
    v1.position = {start.x, start.y, start.z, 1.0f};
    v1.color = color;
    v2.position = {end.x, end.y, end.z, 1.0f};
    v2.color = color;

    vertices_.push_back(v1);
    vertices_.push_back(v2);
}

void LineRenderer::Clear() {
    vertices_.clear();
}

void LineRenderer::Render() {
    if (vertices_.empty() || !dxCommon_ || !camera_) {
        return;
    }

    // 頂点バッファを更新
    if (vertices_.size() * sizeof(LineVertex) > vertexBuffer_->GetDesc().Width) {
        // バッファサイズが足りない場合は再作成
        CreateBuffers();
    }

    // 頂点データをコピー
    LineVertex* mappedData = nullptr;
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    std::memcpy(mappedData, vertices_.data(), vertices_.size() * sizeof(LineVertex));
    vertexBuffer_->Unmap(0, nullptr);

    // トランスフォーム行列を更新
    Matrix4x4 worldMatrix = MakeIdentity4x4();
    Matrix4x4 viewMatrix = camera_->GetViewMatrix();
    Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
    *transformData_ = Multiply(Multiply(worldMatrix, viewMatrix), projectionMatrix);

    // 描画コマンド
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());

    // トランスフォーム定数バッファをセット
    commandList->SetGraphicsRootConstantBufferView(0, transformBuffer_->GetGPUVirtualAddress());

    // 頂点バッファをセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // プリミティブトポロジーをラインリストに設定
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    // 描画
    commandList->DrawInstanced(static_cast<UINT>(vertices_.size()), 1, 0, 0);
}

void LineRenderer::CreatePipelineState() {
    HRESULT hr;

    // RootSignature作成
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // RootParameter作成
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    // シリアライズ
    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    assert(SUCCEEDED(hr));

    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    // InputLayout設定
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "COLOR";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // シェーダーコンパイル
    ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Line.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Line.PS.hlsl", L"ps_6_0");

    // PipelineState設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();
    pipelineStateDesc.InputLayout = inputLayoutDesc;
    pipelineStateDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
    pipelineStateDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};

    // Rasterizer設定
    pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;  // ライン用アンチエイリアス

    // DepthStencil設定（デバッグラインは常に表示）
    pipelineStateDesc.DepthStencilState.DepthEnable = false;  // Depthテストを無効化
    pipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // BlendState設定（アルファブレンド）
    D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = pipelineStateDesc.BlendState.RenderTarget[0];
    blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blenddesc.BlendEnable = TRUE;
    blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blenddesc.BlendOp = D3D12_BLEND_OP_ADD;
    blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;

    // RenderTarget設定
    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    // プリミティブトポロジー設定（LINE用）
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // PipelineState生成
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void LineRenderer::CreateBuffers() {
    HRESULT hr;

    // 頂点バッファ作成
    size_t vertexBufferSize = sizeof(LineVertex) * kMaxLines * 2;  // 2頂点per line
    vertexBuffer_ = dxCommon_->CreateBufferResource(vertexBufferSize);

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(vertexBufferSize);
    vertexBufferView_.StrideInBytes = sizeof(LineVertex);

    // トランスフォームバッファ作成
    transformBuffer_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
    hr = transformBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));
    assert(SUCCEEDED(hr));
    *transformData_ = MakeIdentity4x4();
}
