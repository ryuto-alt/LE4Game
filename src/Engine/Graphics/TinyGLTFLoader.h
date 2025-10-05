#pragma once

#include <vector>
#include <string>
#include "tiny_gltf.h"
#include "Mymath.h"

class TinyGLTFLoader {
public:
    TinyGLTFLoader() = default;
    ~TinyGLTFLoader() = default;

    // GLB/glTFファイルを読み込み
    bool LoadFromFile(const std::string& filePath, std::vector<ModelData>& outModelDataList);
    
    // エラーメッセージ取得
    const std::string& GetErrorMessage() const { return errorMessage_; }

private:
    // glTFモデルデータ
    tinygltf::Model gltfModel_;
    std::string baseDirectory_;
    std::string errorMessage_;
    std::string warningMessage_;

    // 変換処理
    bool ConvertToModelData(std::vector<ModelData>& outModelDataList);
    void ProcessNode(int nodeIndex, std::vector<ModelData>& outModelDataList, const Matrix4x4& parentTransform = MakeIdentity4x4());
    bool ProcessMesh(int meshIndex, std::vector<ModelData>& outModelDataList, const Matrix4x4& transform);
    bool ProcessPrimitive(const tinygltf::Primitive& primitive, ModelData& outModelData, const Matrix4x4& transform);
    
    // 頂点データ変換
    bool ExtractVertexData(const tinygltf::Primitive& primitive, std::vector<VertexData>& outVertices);
    Vector3 GetVector3(const tinygltf::Accessor& accessor, int index);
    Vector2 GetVector2(const tinygltf::Accessor& accessor, int index);
    
    // マテリアル変換
    bool ConvertMaterial(int materialIndex, MaterialData& outMaterial);
    std::string ResolveTexturePath(int textureIndex);
    
    // ヘルパー関数
    Matrix4x4 GetNodeTransform(const tinygltf::Node& node);
    void LogError(const std::string& message);
    void LogWarning(const std::string& message);
};