#include "ParticleManager.h"
#include "TextureManager.h"
#include <cassert>
#include <algorithm>
#include <d3d12.h>

// 静的メンバ変数の初期化
ParticleManager* ParticleManager::instance_ = nullptr;

ParticleManager* ParticleManager::GetInstance() {
    if (!instance_) {
        instance_ = new ParticleManager();
    }
    return instance_;
}

void ParticleManager::Finalize() {
    if (instance_) {
        instance_->ForceReleaseResources();
        delete instance_;
        instance_ = nullptr;
    }
}

ParticleManager::~ParticleManager() {
    // 全パーティクルグループのリソース解放
    for (auto& [name, group] : particleGroups) {
        if (group.instanceResource && group.instanceData) {
            group.instanceResource->Unmap(0, nullptr);
            group.instanceData = nullptr;
        }
        group.instanceResource.Reset();
    }
    particleGroups.clear();

    // その他のマップされたリソースの解放
    if (materialResource && materialData) {
        materialResource->Unmap(0, nullptr);
        materialData = nullptr;
        materialResource.Reset();
    }
    
    if (directionalLightResource && directionalLightData) {
        directionalLightResource->Unmap(0, nullptr);
        directionalLightData = nullptr;
        directionalLightResource.Reset();
    }

    if (vertexResource) {
        vertexResource.Reset();
    }

    // パイプラインステートとルートシグネチャの解放
    if (pipelineState) {
        pipelineState.Reset();
    }
    if (rootSignature) {
        rootSignature.Reset();
    }
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    // nullptrチェック
    assert(dxCommon);
    assert(srvManager);

    // メンバ変数に保存
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // 乱数エンジンの初期化
    std::random_device seed_gen;
    randomEngine_.seed(seed_gen());

    // グラフィックスパイプラインの初期化
    InitializeGraphicsPipeline();

    // 頂点データの作成（四角形ポリゴン）
    std::vector<VertexData> vertices = {
        { {-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} }, // 左下
        { {-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} }, // 左上
        { { 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} }, // 右下
        { { 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} }, // 右上
    };

    // 頂点リソースの作成
    vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * vertices.size());

    // 頂点バッファビューの設定
    vbView.BufferLocation = vertexResource->GetGPUVirtualAddress();
    vbView.SizeInBytes = sizeof(VertexData) * static_cast<UINT>(vertices.size());
    vbView.StrideInBytes = sizeof(VertexData);

    // 頂点データの書き込み
    VertexData* vertexData = nullptr;
    vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    std::memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
    vertexResource->Unmap(0, nullptr);

    // マテリアルリソースの作成
    materialResource = dxCommon_->CreateBufferResource(sizeof(Material));

    // マテリアルデータの書き込み
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    // PBRマテリアルの初期化（パーティクル用）
    materialData->baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData->metallicFactor = 0.0f;
    materialData->roughnessFactor = 1.0f;
    materialData->normalScale = 1.0f;
    materialData->occlusionStrength = 1.0f;
    materialData->emissiveFactor = { 0.0f, 0.0f, 0.0f };
    materialData->alphaCutoff = 0.5f;
    materialData->hasBaseColorTexture = 1; // パーティクルはテクスチャ使用
    materialData->hasMetallicRoughnessTexture = 0;
    materialData->hasNormalTexture = 0;
    materialData->hasOcclusionTexture = 0;
    materialData->hasEmissiveTexture = 0;
    materialData->enableLighting = 0; // ライティングなし
    materialData->alphaMode = 2; // BLEND（半透明）
    materialData->doubleSided = 0;
    materialData->uvTransform = MakeIdentity4x4(); // UVトランスフォームは単位行列

    // ディレクショナルライトリソースの作成
    directionalLightResource = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));

    // ディレクショナルライトデータの書き込み
    directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
    directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白色光
    directionalLightData->direction = { 0.0f, -1.0f, 0.0f }; // 下向き
    directionalLightData->intensity = 1.0f; // 通常の強度
}

void ParticleManager::InitializeGraphicsPipeline() {
    // シェーダーの読み込み - パスを修正してシェーダーを正しく読み込む
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Particle.PS.hlsl", L"ps_6_0");

    // 頂点レイアウト - 新しいシェーダーの入力に合わせる
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // ブレンド設定
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = false;
    blendDesc.IndependentBlendEnable = false;
    blendDesc.RenderTarget[0].BlendEnable = true;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE; // 加算合成
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    // サンプルマスクを追加
    blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
    blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;

    // ラスタライザー設定
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // 深度設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = false; // 深度テストを無効化

    // ルートパラメータの設定 - シェーダーに合わせて修正（4つのパラメータを使用）
    D3D12_ROOT_PARAMETER rootParameters[4] = {}; // 4つのパラメータに変更

    // マテリアル用（b0, PS）
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ディレクショナルライト用（b1, PS）
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 1;
    rootParameters[1].Descriptor.RegisterSpace = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // テクスチャ用（t0, PS）
    D3D12_DESCRIPTOR_RANGE textureRange{};
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.NumDescriptors = 1;
    textureRange.BaseShaderRegister = 0;
    textureRange.RegisterSpace = 0;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &textureRange;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // インスタンシングデータ用（t0, VS）
    D3D12_DESCRIPTOR_RANGE instanceRange{};
    instanceRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    instanceRange.NumDescriptors = 1;
    instanceRange.BaseShaderRegister = 0;
    instanceRange.RegisterSpace = 0;
    instanceRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &instanceRange;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // サンプラーの設定
    D3D12_STATIC_SAMPLER_DESC staticSamplerDesc{};
    staticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplerDesc.ShaderRegister = 0;
    staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ルートシグネチャの設定
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &staticSamplerDesc;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // ルートシグネチャのシリアライズ
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
        rootSignatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
    assert(SUCCEEDED(hr));

    // ルートシグネチャの生成
    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // パイプラインステートの生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
    pipelineDesc.InputLayout = inputLayoutDesc;
    pipelineDesc.pRootSignature = rootSignature.Get();
    pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
    pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
    pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
    pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize();
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.SampleMask = 0xffffffff; // すべてのサンプルを有効化

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &pipelineDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath) {
    // 既に同名のグループが存在する場合は処理をスキップ
    if (particleGroups.find(name) != particleGroups.end()) {
        // 既存のグループがあることをデバッグ出力
        OutputDebugStringA(("ParticleManager: Group already exists - " + name + "\n").c_str());
        return;
    }

    // 新規パーティクルグループを作成
    ParticleGroup group;
    group.textureFilePath = textureFilePath;
    group.instanceCount = 0;

    // テクスチャの読み込み
    TextureManager::GetInstance()->LoadTexture(textureFilePath);
    // テクスチャのSRVインデックスを取得
    group.textureSrvIndex = TextureManager::GetInstance()->GetSrvIndex(textureFilePath);

    // インスタンシング用リソースの作成（最大10000パーティクル）
    const uint32_t kMaxInstanceCount = 10000;
    group.instanceResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kMaxInstanceCount);

    // マップしてポインタを取得
    group.instanceResource->Map(0, nullptr, reinterpret_cast<void**>(&group.instanceData));

    // インスタンシング用SRVの作成
    group.instanceSrvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVForStructuredBuffer(
        group.instanceSrvIndex,
        group.instanceResource,
        kMaxInstanceCount,
        sizeof(ParticleForGPU));

    // パーティクルグループを登録
    particleGroups[name] = group;

    // 登録成功をデバッグ出力
    OutputDebugStringA(("ParticleManager: Created particle group - " + name + "\n").c_str());
}

void ParticleManager::CalculateBillboardMatrix(const Camera* camera) {
    // カメラのビュー行列から、ビルボード行列を計算
    Matrix4x4 viewMatrix = camera->GetViewMatrix();

    // ビルボード行列の計算（カメラの回転のみ打ち消す行列）
    billboardMatrix = MakeIdentity4x4();

    // X軸
    billboardMatrix.m[0][0] = viewMatrix.m[0][0];
    billboardMatrix.m[0][1] = viewMatrix.m[1][0];
    billboardMatrix.m[0][2] = viewMatrix.m[2][0];

    // Y軸
    billboardMatrix.m[1][0] = viewMatrix.m[0][1];
    billboardMatrix.m[1][1] = viewMatrix.m[1][1];
    billboardMatrix.m[1][2] = viewMatrix.m[2][1];

    // Z軸
    billboardMatrix.m[2][0] = viewMatrix.m[0][2];
    billboardMatrix.m[2][1] = viewMatrix.m[1][2];
    billboardMatrix.m[2][2] = viewMatrix.m[2][2];
}

void ParticleManager::Update(const Camera* camera) {
    // ビルボード行列の計算
    CalculateBillboardMatrix(camera);

    // ビュー行列、プロジェクション行列を取得
    Matrix4x4 viewMatrix = camera->GetViewMatrix();
    Matrix4x4 projectionMatrix = camera->GetProjectionMatrix();
    Matrix4x4 viewProjectionMatrix = camera->GetViewProjectionMatrix();

    // 全パーティクルグループの更新
    for (auto& [name, group] : particleGroups) {
        // インスタンス数をリセット
        group.instanceCount = 0;

        // 各パーティクルの更新
        for (auto it = group.particles.begin(); it != group.particles.end(); ) {
            // 寿命チェック
            it->lifeTime += 1.0f / 60.0f; // 60FPS想定
            if (it->lifeTime >= it->lifeTimeMax) {
                // 寿命が尽きたら削除
                it = group.particles.erase(it);
                continue;
            }

            // パーティクルの更新
            // 速度に加速度を加算
            it->velocity.x += it->accel.x / 60.0f;
            it->velocity.y += it->accel.y / 60.0f;
            it->velocity.z += it->accel.z / 60.0f;

            // 位置に速度を加算
            it->position.x += it->velocity.x / 60.0f;
            it->position.y += it->velocity.y / 60.0f;
            it->position.z += it->velocity.z / 60.0f;

            // 回転を更新
            it->rotation += it->rotationVelocity / 60.0f;

            // 線形補間でサイズと色を更新
            float t = it->lifeTime / it->lifeTimeMax;
            it->size = (1.0f - t) * it->startSize + t * it->endSize;

            it->color.x = (1.0f - t) * it->startColor.x + t * it->endColor.x;
            it->color.y = (1.0f - t) * it->startColor.y + t * it->endColor.y;
            it->color.z = (1.0f - t) * it->startColor.z + t * it->endColor.z;
            it->color.w = (1.0f - t) * it->startColor.w + t * it->endColor.w;

            // スケール、回転、座標を使用して行列を作成
            Vector3 scale = { it->size, it->size, it->size };
            Vector3 rotate = { 0.0f, 0.0f, it->rotation };

            // ワールド行列を計算（ビルボード処理も含む）
            Matrix4x4 matScale = MakeScaleMatrix(scale);
            Matrix4x4 matRotZ = MakeRotateZMatrix(rotate.z);

            // スケール -> 回転 -> ビルボード -> 平行移動
            Matrix4x4 matWorld = matScale;
            matWorld = Multiply(matWorld, matRotZ);
            matWorld = Multiply(matWorld, billboardMatrix);
            matWorld.m[3][0] = it->position.x;
            matWorld.m[3][1] = it->position.y;
            matWorld.m[3][2] = it->position.z;

            // WVP行列を計算
            Matrix4x4 matWVP = Multiply(matWorld, viewProjectionMatrix);

            // インスタンシングデータの書き込み（新しいシェーダー形式に合わせて）
            group.instanceData[group.instanceCount].WVP = matWVP;
            group.instanceData[group.instanceCount].World = matWorld;
            group.instanceData[group.instanceCount].color = it->color;

            // インスタンス数をインクリメント
            group.instanceCount++;

            // 次のパーティクルへ
            ++it;
        }
    }
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count) {
    // 詳細設定版のEmitを呼び出し
    Emit(
        name,
        position,
        count,
        { -1.0f, -1.0f, -1.0f },  // velocityMin
        { 1.0f, 1.0f, 1.0f },     // velocityMax
        { 0.0f, 0.0f, 0.0f },     // accelMin
        { 0.0f, -9.8f, 0.0f },    // accelMax
        0.5f,                     // startSizeMin
        1.0f,                     // startSizeMax
        0.0f,                     // endSizeMin
        0.0f,                     // endSizeMax
        { 1.0f, 1.0f, 1.0f, 1.0f }, // startColorMin
        { 1.0f, 1.0f, 1.0f, 1.0f }, // startColorMax
        { 1.0f, 1.0f, 1.0f, 0.0f }, // endColorMin
        { 1.0f, 1.0f, 1.0f, 0.0f }, // endColorMax
        0.0f,                     // rotationMin
        0.0f,                     // rotationMax
        0.0f,                     // rotationVelocityMin
        0.0f,                     // rotationVelocityMax
        1.0f,                     // lifeTimeMin
        3.0f                      // lifeTimeMax
    );
}

void ParticleManager::Emit(
    const std::string& name,
    const Vector3& position,
    uint32_t count,
    const Vector3& velocityMin,
    const Vector3& velocityMax,
    const Vector3& accelMin,
    const Vector3& accelMax,
    float startSizeMin,
    float startSizeMax,
    float endSizeMin,
    float endSizeMax,
    const Vector4& startColorMin,
    const Vector4& startColorMax,
    const Vector4& endColorMin,
    const Vector4& endColorMax,
    float rotationMin,
    float rotationMax,
    float rotationVelocityMin,
    float rotationVelocityMax,
    float lifeTimeMin,
    float lifeTimeMax) {

    // 指定された名前のパーティクルグループが存在するか確認
    auto it = particleGroups.find(name);
    assert(it != particleGroups.end());

    // パラメータのバリデーション（最小値 <= 最大値であることを確認）
    auto clampMinMax = [](float& min, float& max) {
        if (min > max) {
            std::swap(min, max);
        }
    };
    
    // ローカル変数でコピーしてバリデーション
    Vector3 validVelocityMin = velocityMin;
    Vector3 validVelocityMax = velocityMax;
    Vector3 validAccelMin = accelMin;
    Vector3 validAccelMax = accelMax;
    Vector4 validStartColorMin = startColorMin;
    Vector4 validStartColorMax = startColorMax;
    Vector4 validEndColorMin = endColorMin;
    Vector4 validEndColorMax = endColorMax;
    
    float validStartSizeMin = startSizeMin;
    float validStartSizeMax = startSizeMax;
    float validEndSizeMin = endSizeMin;
    float validEndSizeMax = endSizeMax;
    float validRotationMin = rotationMin;
    float validRotationMax = rotationMax;
    float validRotationVelocityMin = rotationVelocityMin;
    float validRotationVelocityMax = rotationVelocityMax;
    float validLifeTimeMin = lifeTimeMin;
    float validLifeTimeMax = lifeTimeMax;
    
    // バリデーションと修正
    clampMinMax(validVelocityMin.x, validVelocityMax.x);
    clampMinMax(validVelocityMin.y, validVelocityMax.y);
    clampMinMax(validVelocityMin.z, validVelocityMax.z);
    
    clampMinMax(validAccelMin.x, validAccelMax.x);
    clampMinMax(validAccelMin.y, validAccelMax.y);
    clampMinMax(validAccelMin.z, validAccelMax.z);
    
    clampMinMax(validStartSizeMin, validStartSizeMax);
    clampMinMax(validEndSizeMin, validEndSizeMax);
    
    clampMinMax(validStartColorMin.x, validStartColorMax.x);
    clampMinMax(validStartColorMin.y, validStartColorMax.y);
    clampMinMax(validStartColorMin.z, validStartColorMax.z);
    clampMinMax(validStartColorMin.w, validStartColorMax.w);
    
    clampMinMax(validEndColorMin.x, validEndColorMax.x);
    clampMinMax(validEndColorMin.y, validEndColorMax.y);
    clampMinMax(validEndColorMin.z, validEndColorMax.z);
    clampMinMax(validEndColorMin.w, validEndColorMax.w);
    
    clampMinMax(validRotationMin, validRotationMax);
    clampMinMax(validRotationVelocityMin, validRotationVelocityMax);
    clampMinMax(validLifeTimeMin, validLifeTimeMax);

    // 均等分布乱数生成器
    std::uniform_real_distribution<float> velocityDistX(validVelocityMin.x, validVelocityMax.x);
    std::uniform_real_distribution<float> velocityDistY(validVelocityMin.y, validVelocityMax.y);
    std::uniform_real_distribution<float> velocityDistZ(validVelocityMin.z, validVelocityMax.z);

    std::uniform_real_distribution<float> accelDistX(validAccelMin.x, validAccelMax.x);
    std::uniform_real_distribution<float> accelDistY(validAccelMin.y, validAccelMax.y);
    std::uniform_real_distribution<float> accelDistZ(validAccelMin.z, validAccelMax.z);

    std::uniform_real_distribution<float> startSizeDist(validStartSizeMin, validStartSizeMax);
    std::uniform_real_distribution<float> endSizeDist(validEndSizeMin, validEndSizeMax);

    std::uniform_real_distribution<float> startColorDistR(validStartColorMin.x, validStartColorMax.x);
    std::uniform_real_distribution<float> startColorDistG(validStartColorMin.y, validStartColorMax.y);
    std::uniform_real_distribution<float> startColorDistB(validStartColorMin.z, validStartColorMax.z);
    std::uniform_real_distribution<float> startColorDistA(validStartColorMin.w, validStartColorMax.w);

    std::uniform_real_distribution<float> endColorDistR(validEndColorMin.x, validEndColorMax.x);
    std::uniform_real_distribution<float> endColorDistG(validEndColorMin.y, validEndColorMax.y);
    std::uniform_real_distribution<float> endColorDistB(validEndColorMin.z, validEndColorMax.z);
    std::uniform_real_distribution<float> endColorDistA(validEndColorMin.w, validEndColorMax.w);

    std::uniform_real_distribution<float> rotationDist(validRotationMin, validRotationMax);
    std::uniform_real_distribution<float> rotationVelocityDist(validRotationVelocityMin, validRotationVelocityMax);

    std::uniform_real_distribution<float> lifeTimeDist(validLifeTimeMin, validLifeTimeMax);

    // 指定された数のパーティクルを生成
    for (uint32_t i = 0; i < count; ++i) {
        Particle particle;

        // 座標
        particle.position = position;

        // 速度（ランダム）
        particle.velocity.x = velocityDistX(randomEngine_);
        particle.velocity.y = velocityDistY(randomEngine_);
        particle.velocity.z = velocityDistZ(randomEngine_);

        // 加速度（ランダム）
        particle.accel.x = accelDistX(randomEngine_);
        particle.accel.y = accelDistY(randomEngine_);
        particle.accel.z = accelDistZ(randomEngine_);

        // サイズ（ランダム）
        particle.startSize = startSizeDist(randomEngine_);
        particle.endSize = endSizeDist(randomEngine_);
        particle.size = particle.startSize;

        // 色（ランダム）
        particle.startColor.x = startColorDistR(randomEngine_);
        particle.startColor.y = startColorDistG(randomEngine_);
        particle.startColor.z = startColorDistB(randomEngine_);
        particle.startColor.w = startColorDistA(randomEngine_);

        particle.endColor.x = endColorDistR(randomEngine_);
        particle.endColor.y = endColorDistG(randomEngine_);
        particle.endColor.z = endColorDistB(randomEngine_);
        particle.endColor.w = endColorDistA(randomEngine_);

        particle.color = particle.startColor;

        // 回転（ランダム）
        particle.rotation = rotationDist(randomEngine_);
        particle.rotationVelocity = rotationVelocityDist(randomEngine_);

        // 寿命（ランダム）
        particle.lifeTimeMax = lifeTimeDist(randomEngine_);
        particle.lifeTime = 0.0f;

        // パーティクルリストに追加
        it->second.particles.push_back(particle);
    }
}

void ParticleManager::Draw() {
    // パーティクルがない場合は描画しない
    bool hasParticles = false;
    for (auto& [name, group] : particleGroups) {
        if (!group.particles.empty()) {
            hasParticles = true;
            break;
        }
    }
    if (!hasParticles) {
        return;
    }

    // コマンドリストの取得
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // パイプラインステートとルートシグネチャをセット
    commandList->SetPipelineState(pipelineState.Get());
    commandList->SetGraphicsRootSignature(rootSignature.Get());

    // プリミティブトポロジーをセット
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // 頂点バッファをセット
    commandList->IASetVertexBuffers(0, 1, &vbView);

    // マテリアルとディレクショナルライトをセット
    commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());

    // 各パーティクルグループの描画
    for (auto& [name, group] : particleGroups) {
        // パーティクルがない場合はスキップ
        if (group.particles.empty() || group.instanceCount == 0) {
            continue;
        }

        // テクスチャをセット（ピクセルシェーダー用）
        srvManager_->SetGraphicsRootDescriptorTable(2, group.textureSrvIndex);

        // インスタンシングデータをセット（頂点シェーダー用）
        srvManager_->SetGraphicsRootDescriptorTable(3, group.instanceSrvIndex);

        // 描画（インスタンシング）
        commandList->DrawInstanced(4, group.instanceCount, 0, 0);
    }
}

// デバッグ用：シンプルな四角形を描画
void ParticleManager::DrawSimpleQuad() {
    // コマンドリストの取得
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // パイプラインステートとルートシグネチャをセット
    commandList->SetPipelineState(pipelineState.Get());
    commandList->SetGraphicsRootSignature(rootSignature.Get());

    // プリミティブトポロジーをセット
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // 頂点バッファをセット
    commandList->IASetVertexBuffers(0, 1, &vbView);

    // マテリアルとディレクショナルライトをセット
    commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, directionalLightResource->GetGPUVirtualAddress());

    // テクスチャのテスト用にsmoke.pngを使用
    auto it = particleGroups.find("smoke");
    if (it != particleGroups.end()) {
        // テクスチャをセット
        srvManager_->SetGraphicsRootDescriptorTable(2, it->second.textureSrvIndex);

        // 単純な四角形を描画
        commandList->DrawInstanced(4, 1, 0, 0);
    }
}