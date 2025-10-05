#pragma once
#include <string>
#include <vector>
#include "Mymath.h"
#include <d3d12.h>
#include <wrl.h>
#include "DirectXCommon.h"
#include "Mymath.h"

// モデルデータクラス
class Model {
public:
    // コンストラクタ
    Model();
    // デストラクタ
    ~Model();

    // 初期化
    void Initialize(DirectXCommon* dxCommon);

    // モデルの読み込み
    void LoadFromObj(const std::string& directoryPath, const std::string& filename);
    void LoadFromGltf(const std::string& directoryPath, const std::string& filename);

    // アクセサ
    const std::vector<VertexData>& GetVertices() const { return modelData_.vertices; }
    uint32_t GetVertexCount() const { return static_cast<uint32_t>(modelData_.vertices.size()); }
    const MaterialData& GetMaterial() const { return modelData_.material; }
    const std::string& GetTextureFilePath() const { return modelData_.material.textureFilePath; }
    const D3D12_VERTEX_BUFFER_VIEW& GetVBView() const { return vertexBufferView_; }
    ID3D12Resource* GetVertexResource() const { return vertexResource_.Get(); }
    const ModelData& GetModelData() const { return modelData_; }
    
    // マルチマテリアル対応のためのパブリックアクセサ
    ModelData& GetModelDataInternal() { return modelData_; }
    const std::vector<D3D12_VERTEX_BUFFER_VIEW>& GetVertexBufferViews() const { return vertexBufferViews_; }

    // 頂点バッファの作成（マルチマテリアル対応）
    void CreateVertexBuffer();

protected:

private:
    // モデルデータの最適化（UV球など改善のため）
    void OptimizeTriangles(ModelData& modelData, const std::string& filename);

    // モデルデータの読み込み
    ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
    ModelData LoadGltfFile(const std::string& directoryPath, const std::string& filename);
    // マテリアルデータの読み込み
    MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    // モデルデータ
    ModelData modelData_;
    // 頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    // 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    // DirectXCommon
    DirectXCommon* dxCommon_;

    // マルチマテリアル対応
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> vertexResources_;
    std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferViews_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> indexResources_;
    std::vector<D3D12_INDEX_BUFFER_VIEW> indexBufferViews_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> materialTemplateResources_;
};