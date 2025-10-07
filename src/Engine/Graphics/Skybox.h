#pragma once
#include "Mymath.h"
#include "Camera.h"
#include "Mymath.h"
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <memory>

class DirectXCommon;
class SrvManager;
class TextureManager;

struct SkyboxVertex {
    Vector3 position;
};

class Skybox {
public:
    Skybox();
    ~Skybox();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, TextureManager* textureManager);
    void LoadCubemap(const std::string& filePath);
    void Update();
    void Draw(Camera* camera);

    void SetScale(float scale) { scale_ = scale; }
    float GetScale() const { return scale_; }

    // 環境マップの自動管理機能
    static const std::string& GetCurrentEnvironmentTexturePath();
    static bool IsEnvironmentTextureLoaded();
    
    // 環境マップ自動適用機能（簡易版）
    template<typename T>
    static void SetEnvMap(T* object) {
        if (object && IsEnvironmentTextureLoaded()) {
            object->SetEnvTex(GetCurrentEnvironmentTexturePath());
        }
    }

    template<typename T>
    static void SetEnvMap(std::unique_ptr<T>& object) {
        if (object && IsEnvironmentTextureLoaded()) {
            object->SetEnvTex(GetCurrentEnvironmentTexturePath());
        }
    }
    
    // 後方互換性のため古い名前も残す
    template<typename T>
    static void ApplyEnvironmentMapTo(T* object) { SetEnvMap(object); }
    
    template<typename T>
    static void ApplyEnvironmentMapTo(std::unique_ptr<T>& object) { SetEnvMap(object); }

private:
    void CreateVertexData();
    void CreateIndexData();
    void CreateMaterial();
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

    SkyboxVertex* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    Material* materialData_ = nullptr;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    struct CameraData {
        Vector3 worldPosition;
        float fisheyeStrength;
    };
    CameraData* cameraData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

    std::string cubemapFilePath_;
    uint32_t cubemapSrvIndex_ = UINT32_MAX; // 無効値で初期化
    float scale_ = 1000.0f;
    bool cubemapLoaded_ = false;

    // 環境マップの自動管理用の静的変数
    static std::string currentEnvironmentTexturePath_;
    static bool environmentTextureLoaded_;

    static const uint32_t kNumVertices = 8;
    static const uint32_t kNumIndices = 36;
};