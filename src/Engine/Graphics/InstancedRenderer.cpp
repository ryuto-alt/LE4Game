#include "InstancedRenderer.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Camera.h"
#include "TextureManager.h"
#include <cassert>

InstancedRenderer::InstancedRenderer()
    : dxCommon_(nullptr), spriteCommon_(nullptr), instanceDataPtr_(nullptr),
      maxInstances_(0), instanceCount_(0), materialData_(nullptr),
      directionalLightData_(nullptr), spotLightData_(nullptr), cameraData_(nullptr),
      viewProjectionData_(nullptr), lod0Distance_(50.0f), lod1Distance_(150.0f), lod2Distance_(300.0f) {
}

InstancedRenderer::~InstancedRenderer() {
    if (instanceBuffer_) {
        instanceBuffer_->Unmap(0, nullptr);
        instanceBuffer_.Reset();
    }
}

void InstancedRenderer::Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon, size_t maxInstances) {
    assert(dxCommon);
    assert(spriteCommon);

    dxCommon_ = dxCommon;
    spriteCommon_ = spriteCommon;
    maxInstances_ = maxInstances;
    instanceCount_ = 0;

    // インスタンスバッファの作成
    instanceBuffer_ = dxCommon_->CreateBufferResource(sizeof(InstanceData) * maxInstances_);
    instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&instanceDataPtr_));

    // インスタンスバッファビューの設定
    instanceBufferView_.BufferLocation = instanceBuffer_->GetGPUVirtualAddress();
    instanceBufferView_.SizeInBytes = static_cast<UINT>(sizeof(InstanceData) * maxInstances_);
    instanceBufferView_.StrideInBytes = sizeof(InstanceData);

    // マテリアルリソースの作成
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    materialData_->metallicFactor = 0.0f;
    materialData_->roughnessFactor = 1.0f;
    materialData_->normalScale = 1.0f;
    materialData_->occlusionStrength = 1.0f;
    materialData_->emissiveFactor = {0.0f, 0.0f, 0.0f};
    materialData_->alphaCutoff = 0.5f;
    materialData_->hasBaseColorTexture = 0;
    materialData_->hasMetallicRoughnessTexture = 0;
    materialData_->hasNormalTexture = 0;
    materialData_->hasOcclusionTexture = 0;
    materialData_->hasEmissiveTexture = 0;
    materialData_->enableLighting = 1;
    materialData_->alphaMode = 0;
    materialData_->doubleSided = 0;
    materialData_->enableEnvironmentMap = 1;
    materialData_->environmentMapIntensity = 0.3f;
    materialData_->uvTransform = MakeIdentity4x4();

    // ライトリソースの作成
    directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

    spotLightResource_ = dxCommon_->CreateBufferResource(sizeof(SpotLight));
    spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

    // カメラリソースの作成
    cameraResource_ = dxCommon_->CreateBufferResource(sizeof(Vector3));
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

    // ViewProjectionマトリックスリソースの作成
    viewProjectionResource_ = dxCommon_->CreateBufferResource(sizeof(ViewProjectionMatrix));
    viewProjectionResource_->Map(0, nullptr, reinterpret_cast<void**>(&viewProjectionData_));
}

void InstancedRenderer::AddInstance(const Matrix4x4& worldMatrix, const Vector4& color) {
    if (instanceCount_ >= maxInstances_) {
        OutputDebugStringA("Warning: Instance buffer is full!\n");
        return;
    }

    instanceDataPtr_[instanceCount_].world = worldMatrix;
    instanceDataPtr_[instanceCount_].color = color;
    instanceCount_++;
}

void InstancedRenderer::AddInstanceWithLOD(const Vector3& position, const Matrix4x4& worldMatrix, const Vector3& cameraPos, const Vector4& color) {
    if (instanceCount_ >= maxInstances_) {
        OutputDebugStringA("Warning: Instance buffer is full!\n");
        return;
    }

    // カメラからの距離を計算
    float dx = position.x - cameraPos.x;
    float dy = position.y - cameraPos.y;
    float dz = position.z - cameraPos.z;
    float distanceSq = dx * dx + dy * dy + dz * dz;

    // LOD判定（距離の2乗で比較して高速化）
    float lod0DistSq = lod0Distance_ * lod0Distance_;
    float lod1DistSq = lod1Distance_ * lod1Distance_;
    float lod2DistSq = lod2Distance_ * lod2Distance_;

    // LOD0: 近距離 - 全て描画
    if (distanceSq < lod0DistSq) {
        instanceDataPtr_[instanceCount_].world = worldMatrix;
        instanceDataPtr_[instanceCount_].color = color;
        instanceCount_++;
    }
    // LOD1: 中距離 - 50%間引き（市松模様パターン）
    else if (distanceSq < lod1DistSq) {
        // 位置のハッシュ値を使って決定論的に50%間引き
        int hash = static_cast<int>(position.x * 100.0f) + static_cast<int>(position.z * 100.0f);
        if (hash % 2 == 0) {
            instanceDataPtr_[instanceCount_].world = worldMatrix;
            instanceDataPtr_[instanceCount_].color = color;
            instanceCount_++;
        }
    }
    // LOD2: 遠距離 - 75%間引き（4つに1つだけ描画）
    else if (distanceSq < lod2DistSq) {
        int hash = static_cast<int>(position.x * 100.0f) + static_cast<int>(position.z * 100.0f);
        if (hash % 4 == 0) {
            instanceDataPtr_[instanceCount_].world = worldMatrix;
            instanceDataPtr_[instanceCount_].color = color;
            instanceCount_++;
        }
    }
    // それ以上の距離 - 描画しない
}

void InstancedRenderer::Clear() {
    instanceCount_ = 0;
}

void InstancedRenderer::Draw(Model* model, Camera* camera, const DirectionalLight& dirLight, const SpotLight& spotLight) {
    if (instanceCount_ == 0 || !model || !camera) {
        return;
    }

    assert(dxCommon_);

    // ライトデータの更新
    *directionalLightData_ = dirLight;
    *spotLightData_ = spotLight;

    // カメラデータの更新
    *cameraData_ = camera->GetTranslate();

    // ViewProjectionマトリックスの更新
    viewProjectionData_->viewProjection = Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());

    // ルートシグネチャの設定
    dxCommon_->GetCommandList()->SetGraphicsRootSignature(spriteCommon_->GetRootSignature().Get());

    // PBRインスタンシング用パイプラインの設定
    dxCommon_->GetCommandList()->SetPipelineState(spriteCommon_->GetPBRInstancedPipelineState().Get());

    dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点バッファとインスタンスバッファの設定
    D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
        model->GetVBView(),
        instanceBufferView_
    };
    dxCommon_->GetCommandList()->IASetVertexBuffers(0, 2, vbvs);

    // 共通リソースの設定
    dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());
    dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
    dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(5, spotLightResource_->GetGPUVirtualAddress());
    dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(7, cameraResource_->GetGPUVirtualAddress());

    // ベーステクスチャの設定（t0）
    dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2,
        TextureManager::GetInstance()->GetSrvHandleGPU("white1x1.png"));

    // 環境マップテクスチャ（t2）- 初期化済み前提で毎フレームチェック不要
    static const std::string envTexturePath = "Resources/Models/skybox/warm_restaurant_night_2k.hdr";
    dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(6,
        TextureManager::GetInstance()->GetSrvHandleGPU(envTexturePath));

    // マルチメッシュのインスタンシング描画
    const ModelData& modelData = model->GetModelData();

    // マルチマテリアルモデルの場合、全メッシュを描画
    if (!modelData.matVertexData.empty()) {
        const auto& vertexBufferViews = model->GetVertexBufferViews();

        size_t meshIndex = 0;
        for (auto it = modelData.matVertexData.begin(); it != modelData.matVertexData.end(); ++it, ++meshIndex) {
            if (meshIndex >= vertexBufferViews.size()) break;

            const MaterialVertexData& meshData = it->second;

            // マテリアル情報を更新（各メッシュのマテリアルを使用）
            if (meshData.materialIndex < modelData.materials.size()) {
                const MaterialData& meshMaterial = modelData.materials[meshData.materialIndex];

                materialData_->baseColorFactor = meshMaterial.baseColorFactor;
                materialData_->metallicFactor = meshMaterial.metallicFactor;
                materialData_->roughnessFactor = meshMaterial.roughnessFactor;
                materialData_->normalScale = meshMaterial.normalScale;
                materialData_->occlusionStrength = meshMaterial.occlusionStrength;
                materialData_->emissiveFactor = meshMaterial.emissiveFactor;
                materialData_->alphaCutoff = meshMaterial.alphaCutoff;
                materialData_->hasBaseColorTexture = 0;

                // テクスチャの設定
                if (!meshMaterial.textureFilePath.empty()) {
                    std::string texturePath = meshMaterial.textureFilePath;
                    if (TextureManager::GetInstance()->IsTextureExists(texturePath)) {
                        materialData_->hasBaseColorTexture = 1;
                        dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2,
                            TextureManager::GetInstance()->GetSrvHandleGPU(texturePath));
                    }
                } else {
                    // テクスチャがない場合は白テクスチャを使用
                    dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2,
                        TextureManager::GetInstance()->GetSrvHandleGPU("white1x1.png"));
                }
            }

            // 各メッシュの頂点バッファを設定
            D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
                vertexBufferViews[meshIndex],
                instanceBufferView_
            };
            dxCommon_->GetCommandList()->IASetVertexBuffers(0, 2, vbvs);

            // このメッシュの頂点数を取得
            UINT vertexCount = static_cast<UINT>(meshData.vertices.size());

            if (vertexCount > 0) {
                dxCommon_->GetCommandList()->DrawInstanced(
                    vertexCount,
                    static_cast<UINT>(instanceCount_),
                    0, 0
                );
            }
        }
    } else if (!modelData.vertices.empty()) {
        // シングルメッシュモデル（元のマテリアルデータを使用）
        const MaterialData& modelMaterial = modelData.material;

        // モデルのマテリアル情報をコピー
        materialData_->baseColorFactor = modelMaterial.baseColorFactor;
        materialData_->metallicFactor = modelMaterial.metallicFactor;
        materialData_->roughnessFactor = modelMaterial.roughnessFactor;
        materialData_->normalScale = modelMaterial.normalScale;
        materialData_->occlusionStrength = modelMaterial.occlusionStrength;
        materialData_->emissiveFactor = modelMaterial.emissiveFactor;
        materialData_->alphaCutoff = modelMaterial.alphaCutoff;
        materialData_->hasBaseColorTexture = 0;

        // テクスチャの設定
        if (!modelMaterial.textureFilePath.empty()) {
            std::string texturePath = modelMaterial.textureFilePath;
            if (TextureManager::GetInstance()->IsTextureExists(texturePath)) {
                materialData_->hasBaseColorTexture = 1;
                dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2,
                    TextureManager::GetInstance()->GetSrvHandleGPU(texturePath));
            }
        } else {
            // テクスチャがない場合は白テクスチャを使用
            dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2,
                TextureManager::GetInstance()->GetSrvHandleGPU("white1x1.png"));
        }

        UINT vertexCount = static_cast<UINT>(modelData.vertices.size());
        if (vertexCount > 0) {
            dxCommon_->GetCommandList()->DrawInstanced(
                vertexCount,
                static_cast<UINT>(instanceCount_),
                0, 0
            );
        }
    }
}
