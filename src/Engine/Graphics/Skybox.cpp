#include "Skybox.h"
#include "DirectXCommon.h"
#include "SRVManager.h"
#include "TextureManager.h"
#include "Mymath.h"
#include <cassert>

// 静的変数の定義
std::string Skybox::currentEnvironmentTexturePath_ = "";
bool Skybox::environmentTextureLoaded_ = false;

Skybox::Skybox() {}

Skybox::~Skybox() {}

void Skybox::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, TextureManager* textureManager) {
    assert(dxCommon);
    assert(srvManager);
    assert(textureManager);
    
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    textureManager_ = textureManager;

    CreateVertexData();
    CreateIndexData();
    CreateMaterial();
    CreateRootSignature();
    CreateGraphicsPipelineState();
}

void Skybox::CreateVertexData() {
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(SkyboxVertex) * kNumVertices);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(SkyboxVertex) * kNumVertices;
    vertexBufferView_.StrideInBytes = sizeof(SkyboxVertex);

    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    // 立方体の8つの頂点（-1～1の範囲）
    vertexData_[0].position = { -1.0f,  1.0f, -1.0f }; // 左上前
    vertexData_[1].position = {  1.0f,  1.0f, -1.0f }; // 右上前
    vertexData_[2].position = { -1.0f, -1.0f, -1.0f }; // 左下前
    vertexData_[3].position = {  1.0f, -1.0f, -1.0f }; // 右下前
    vertexData_[4].position = { -1.0f,  1.0f,  1.0f }; // 左上後
    vertexData_[5].position = {  1.0f,  1.0f,  1.0f }; // 右上後
    vertexData_[6].position = { -1.0f, -1.0f,  1.0f }; // 左下後
    vertexData_[7].position = {  1.0f, -1.0f,  1.0f }; // 右下後
}

void Skybox::CreateIndexData() {
    indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * kNumIndices);
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * kNumIndices;

    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

    // 内側から見るためのインデックス（時計回り）
    uint32_t indices[kNumIndices] = {
        // 前面
        0, 2, 1, 1, 2, 3,
        // 右面  
        1, 3, 5, 5, 3, 7,
        // 後面
        5, 7, 4, 4, 7, 6,
        // 左面
        4, 6, 0, 0, 6, 2,
        // 上面
        4, 0, 5, 5, 0, 1,
        // 下面
        2, 6, 3, 3, 6, 7
    };

    for (uint32_t i = 0; i < kNumIndices; ++i) {
        indexData_[i] = indices[i];
    }
}

void Skybox::CreateMaterial() {
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    // PBRマテリアルの初期化（Skybox用）
    materialData_->baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->metallicFactor = 0.0f;
    materialData_->roughnessFactor = 1.0f;
    materialData_->normalScale = 1.0f;
    materialData_->occlusionStrength = 1.0f;
    materialData_->emissiveFactor = { 0.0f, 0.0f, 0.0f };
    materialData_->alphaCutoff = 0.5f;
    materialData_->hasBaseColorTexture = 1; // Skyboxはテクスチャ使用
    materialData_->hasMetallicRoughnessTexture = 0;
    materialData_->hasNormalTexture = 0;
    materialData_->hasOcclusionTexture = 0;
    materialData_->hasEmissiveTexture = 0;
    materialData_->enableLighting = 0; // Skyboxはライティング無効
    materialData_->alphaMode = 0; // OPAQUE
    materialData_->doubleSided = 0;
    materialData_->uvTransform = MakeIdentity4x4();

    transformationMatrixResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

    // カメラデータリソースの作成
    cameraResource_ = dxCommon_->CreateBufferResource(sizeof(CameraData));
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
    cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
    cameraData_->fisheyeStrength = 0.0f;
}

void Skybox::CreateRootSignature() {
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].RegisterSpace = 0;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    // カメラデータ用のCBV（b3レジスタ）
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[3].Descriptor.ShaderRegister = 3;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void Skybox::CreateGraphicsPipelineState() {
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Skybox.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Skybox.PS.hlsl", L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // Skybox特有のDepthStencil設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度書き込み無効
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 最遠方として描画
    depthStencilDesc.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));
}

void Skybox::LoadCubemap(const std::string& filePath) {
    // 初期化状態をリセット
    cubemapLoaded_ = false;
    cubemapSrvIndex_ = UINT32_MAX;
    cubemapFilePath_ = filePath;

    // 基本的な安全性チェック
    if (!textureManager_) {
        OutputDebugStringA("Skybox: TextureManagerが無効です\n");
        environmentTextureLoaded_ = false;
        return;
    }

    // ファイルの存在確認
    DWORD fileAttributes = GetFileAttributesA(filePath.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        OutputDebugStringA(("Skybox: ファイルが見つかりません: " + filePath + "\n").c_str());
        environmentTextureLoaded_ = false;
        return;
    }

    // ファイル形式を判定
    std::string fileType = "Unknown";
    if (filePath.ends_with(".dds")) {
        fileType = "DDS";
    } else if (filePath.ends_with(".hdr")) {
        fileType = "HDR";
    }

    OutputDebugStringA(("Skybox: " + fileType + "ファイルを読み込み中: " + filePath + "\n").c_str());

    // Cubemapファイルを読み込み (DDS/HDR両方対応)
    bool loadResult = textureManager_->LoadTexture(filePath);
    if (!loadResult) {
        OutputDebugStringA(("Skybox: " + fileType + "ファイルの読み込みに失敗しました: " + filePath + "\n").c_str());
        environmentTextureLoaded_ = false;
        return;
    }

    // CubemapのSRVインデックスを取得
    uint32_t srvIndex = textureManager_->GetSrvIndex(filePath);
    if (srvIndex == UINT32_MAX) {
        OutputDebugStringA(("Skybox: 無効なSRVインデックスが返されました: " + filePath + "\n").c_str());
        environmentTextureLoaded_ = false;
        return;
    }

    // 全ての検証が成功した場合のみ設定を適用
    cubemapSrvIndex_ = srvIndex;
    cubemapLoaded_ = true;

    // 環境マップの自動管理機能：現在のテクスチャパスを静的変数に保存
    currentEnvironmentTexturePath_ = filePath;
    environmentTextureLoaded_ = true;

    OutputDebugStringA(("Skybox: " + fileType + " Cubemap読み込み成功: " + filePath + ", SRVIndex: " + std::to_string(cubemapSrvIndex_) + "\n").c_str());
    OutputDebugStringA(("Skybox: 環境マップが自動設定されました: " + filePath + "\n").c_str());
}

void Skybox::Update() {
    // 特に更新処理は不要
}

void Skybox::Draw(Camera* camera) {
    assert(camera);
    
    // 安全性チェック：必要なリソースが全て有効かチェック
    if (!dxCommon_ || !srvManager_ || !rootSignature_ || !graphicsPipelineState_) {
        OutputDebugStringA("Skybox: 必要なリソースが初期化されていません\n");
        return;
    }
    
    // Cubemapが正しく読み込まれていない場合は描画をスキップ
    if (!cubemapLoaded_) {
        OutputDebugStringA("Skybox: Cubemap読み込み未完了のため描画をスキップ\n");
        return;
    }
    
    // SRVインデックスの有効性チェック
    if (cubemapSrvIndex_ == UINT32_MAX) {
        OutputDebugStringA("Skybox: 無効なSRVインデックスのため描画をスキップ\n");
        return;
    }

    // デバッグ出力は初回のみまたは重要な場合のみ
    // OutputDebugStringA(("Skybox: 描画開始 - SRVIndex: " + std::to_string(cubemapSrvIndex_) + ", ファイル: " + cubemapFilePath_ + "\n").c_str());

    // コマンドリストの有効性チェック
    auto commandList = dxCommon_->GetCommandList();
    if (!commandList) {
        OutputDebugStringA("Skybox: CommandListが無効です\n");
        return;
    }

    // ワールド行列（スケールのみ）
    Matrix4x4 worldMatrix = MakeScaleMatrix({ scale_, scale_, scale_ });
    // カメラの位置に移動（平行移動はカメラと一緒に移動）
    Vector3 cameraPos = camera->GetTranslate();
    worldMatrix.m[3][0] = cameraPos.x;
    worldMatrix.m[3][1] = cameraPos.y;
    worldMatrix.m[3][2] = cameraPos.z;

    Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix()));
    transformationMatrixData_->WVP = worldViewProjectionMatrix;
    transformationMatrixData_->World = worldMatrix;

    // カメラデータの更新
    if (cameraData_ && camera) {
        cameraData_->worldPosition = camera->GetTranslate();
        cameraData_->fisheyeStrength = camera->GetFisheyeStrength();
    }

    // ルートシグネチャとパイプラインステートを設定
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(graphicsPipelineState_.Get());
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 定数バッファとSRVを設定
    commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());

    // SRVManagerの有効性を再確認してからディスクリプタテーブルを設定
    if (srvManager_) {
        srvManager_->SetGraphicsRootDescriptorTable(2, cubemapSrvIndex_);
    } else {
        OutputDebugStringA("Skybox: SrvManagerが無効のため、ディスクリプタテーブル設定をスキップ\n");
        return;
    }

    // カメラデータのCBVを設定（b3レジスタ）
    commandList->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());

    commandList->DrawIndexedInstanced(kNumIndices, 1, 0, 0, 0);
    // 描画完了のメッセージは頻繁すぎるため無効化
    // OutputDebugStringA("Skybox: 描画完了\n");
}

// 静的関数の実装
const std::string& Skybox::GetCurrentEnvironmentTexturePath() {
    return currentEnvironmentTexturePath_;
}

bool Skybox::IsEnvironmentTextureLoaded() {
    return environmentTextureLoaded_;
}