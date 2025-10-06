#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include "Mymath.h"
#include "Model.h"

class DirectXCommon;
class SpriteCommon;
class Camera;

// インスタンスごとのデータ
struct InstanceData {
    Matrix4x4 world;
    Vector4 color;
};

// インスタンシング描画クラス
class InstancedRenderer {
public:
    InstancedRenderer();
    ~InstancedRenderer();

    void Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon, size_t maxInstances = 10000);

    // インスタンスの追加
    void AddInstance(const Matrix4x4& worldMatrix, const Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f});

    // インスタンスのクリア
    void Clear();

    // 描画
    void Draw(Model* model, Camera* camera, const DirectionalLight& dirLight, const SpotLight& spotLight);

    // インスタンス数の取得
    size_t GetInstanceCount() const { return instanceCount_; }

private:
    DirectXCommon* dxCommon_;
    SpriteCommon* spriteCommon_;

    // インスタンスバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
    D3D12_VERTEX_BUFFER_VIEW instanceBufferView_;
    InstanceData* instanceDataPtr_;

    size_t maxInstances_;
    size_t instanceCount_;

    // 共通リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_;

    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_;

    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    SpotLight* spotLightData_;

    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    Vector3* cameraData_;

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    struct ViewProjectionMatrix {
        Matrix4x4 viewProjection;
    };
    ViewProjectionMatrix* viewProjectionData_;
};
