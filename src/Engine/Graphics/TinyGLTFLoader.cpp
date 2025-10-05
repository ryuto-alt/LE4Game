#include "TinyGLTFLoader.h"
#include <iostream>
#include <algorithm>
#include <Windows.h>

bool TinyGLTFLoader::LoadFromFile(const std::string& filePath, std::vector<ModelData>& outModelDataList) {
    // ベースディレクトリを設定
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseDirectory_ = filePath.substr(0, lastSlash + 1);
    } else {
        baseDirectory_ = "";
    }

    // tinygltfでglTFを読み込み
    tinygltf::TinyGLTF loader;
    bool ret = false;

    if (filePath.size() >= 5 && filePath.substr(filePath.size() - 5) == ".gltf") {
        ret = loader.LoadASCIIFromFile(&gltfModel_, &errorMessage_, &warningMessage_, filePath);
    } else {
        LogError("Unsupported file format. Only .gltf is supported.");
        return false;
    }

    if (!ret) {
        LogError("Failed to load glTF file: " + errorMessage_);
        return false;
    }

    if (!warningMessage_.empty()) {
        LogWarning(warningMessage_);
    }

    // ModelDataに変換
    return ConvertToModelData(outModelDataList);
}

bool TinyGLTFLoader::ConvertToModelData(std::vector<ModelData>& outModelDataList) {
    outModelDataList.clear();

    // デフォルトシーンを処理
    int sceneIndex = gltfModel_.defaultScene >= 0 ? gltfModel_.defaultScene : 0;
    if (sceneIndex >= gltfModel_.scenes.size()) {
        LogError("Invalid scene index");
        return false;
    }

    const auto& scene = gltfModel_.scenes[sceneIndex];
    
    // ルートノードから再帰的に処理
    for (int nodeIndex : scene.nodes) {
        ProcessNode(nodeIndex, outModelDataList);
    }

    if (outModelDataList.empty()) {
        LogWarning("No mesh data found in glTF file");
        return false;
    }

    return true;
}

void TinyGLTFLoader::ProcessNode(int nodeIndex, std::vector<ModelData>& outModelDataList, const Matrix4x4& parentTransform) {
    if (nodeIndex < 0 || nodeIndex >= gltfModel_.nodes.size()) {
        return;
    }

    const auto& node = gltfModel_.nodes[nodeIndex];
    
    // ノードの変換行列を取得
    Matrix4x4 nodeTransform = GetNodeTransform(node);
    Matrix4x4 worldTransform = Multiply(nodeTransform, parentTransform);

    // メッシュが存在する場合は処理
    if (node.mesh >= 0) {
        ProcessMesh(node.mesh, outModelDataList, worldTransform);
    }

    // 子ノードを再帰的に処理
    for (int childIndex : node.children) {
        ProcessNode(childIndex, outModelDataList, worldTransform);
    }
}

bool TinyGLTFLoader::ProcessMesh(int meshIndex, std::vector<ModelData>& outModelDataList, const Matrix4x4& transform) {
    if (meshIndex < 0 || meshIndex >= gltfModel_.meshes.size()) {
        return false;
    }

    const auto& mesh = gltfModel_.meshes[meshIndex];
    
    // 各プリミティブをModelDataに変換
    for (const auto& primitive : mesh.primitives) {
        ModelData modelData;
        if (ProcessPrimitive(primitive, modelData, transform)) {
            outModelDataList.push_back(std::move(modelData));
        }
    }

    return true;
}

bool TinyGLTFLoader::ProcessPrimitive(const tinygltf::Primitive& primitive, ModelData& outModelData, const Matrix4x4& transform) {
    // 頂点データを抽出
    if (!ExtractVertexData(primitive, outModelData.vertices)) {
        LogError("Failed to extract vertex data");
        return false;
    }

    // 変換行列を適用
    for (auto& vertex : outModelData.vertices) {
        Vector4 transformedPos = MultiplyVector(Vector4{vertex.position.x, vertex.position.y, vertex.position.z, 1.0f}, transform);
        vertex.position = transformedPos;
    }

    // マテリアルを変換
    if (primitive.material >= 0) {
        ConvertMaterial(primitive.material, outModelData.material);
    } else {
        // デフォルトマテリアル
        outModelData.material.isPBR = true;
        outModelData.material.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
        outModelData.material.metallicFactor = 0.0f;
        outModelData.material.roughnessFactor = 1.0f;
    }

    return true;
}

bool TinyGLTFLoader::ExtractVertexData(const tinygltf::Primitive& primitive, std::vector<VertexData>& outVertices) {
    outVertices.clear();

    // 位置データは必須
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) {
        LogError("POSITION attribute not found");
        return false;
    }

    const auto& posAccessor = gltfModel_.accessors[posIt->second];
    size_t vertexCount = posAccessor.count;
    outVertices.resize(vertexCount);

    // 位置データを取得
    for (size_t i = 0; i < vertexCount; ++i) {
        Vector3 position = GetVector3(posAccessor, static_cast<int>(i));
        outVertices[i].position = {position.x, position.y, position.z, 1.0f};
    }

    // 法線データ
    auto normalIt = primitive.attributes.find("NORMAL");
    if (normalIt != primitive.attributes.end()) {
        const auto& normalAccessor = gltfModel_.accessors[normalIt->second];
        for (size_t i = 0; i < vertexCount; ++i) {
            outVertices[i].normal = GetVector3(normalAccessor, static_cast<int>(i));
        }
    } else {
        // デフォルト法線
        for (auto& vertex : outVertices) {
            vertex.normal = {0.0f, 1.0f, 0.0f};
        }
    }

    // UV座標
    auto texcoordIt = primitive.attributes.find("TEXCOORD_0");
    if (texcoordIt != primitive.attributes.end()) {
        const auto& texcoordAccessor = gltfModel_.accessors[texcoordIt->second];
        for (size_t i = 0; i < vertexCount; ++i) {
            outVertices[i].texcoord = GetVector2(texcoordAccessor, static_cast<int>(i));
        }
    } else {
        // デフォルトUV
        for (auto& vertex : outVertices) {
            vertex.texcoord = {0.0f, 0.0f};
        }
    }

    return true;
}

Vector3 TinyGLTFLoader::GetVector3(const tinygltf::Accessor& accessor, int index) {
    const auto& bufferView = gltfModel_.bufferViews[accessor.bufferView];
    const auto& buffer = gltfModel_.buffers[bufferView.buffer];
    
    const float* data = reinterpret_cast<const float*>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset + index * sizeof(float) * 3]);
    
    return {data[0], data[1], data[2]};
}

Vector2 TinyGLTFLoader::GetVector2(const tinygltf::Accessor& accessor, int index) {
    const auto& bufferView = gltfModel_.bufferViews[accessor.bufferView];
    const auto& buffer = gltfModel_.buffers[bufferView.buffer];
    
    const float* data = reinterpret_cast<const float*>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset + index * sizeof(float) * 2]);
    
    return {data[0], data[1]};
}

bool TinyGLTFLoader::ConvertMaterial(int materialIndex, MaterialData& outMaterial) {
    if (materialIndex < 0 || materialIndex >= gltfModel_.materials.size()) {
        return false;
    }

    const auto& gltfMaterial = gltfModel_.materials[materialIndex];
    
    // PBRマテリアルとして設定
    outMaterial.isPBR = true;
    
    // PBRメタリックラフネスプロパティ
    const auto& pbr = gltfMaterial.pbrMetallicRoughness;
    outMaterial.baseColorFactor = {
        static_cast<float>(pbr.baseColorFactor[0]),
        static_cast<float>(pbr.baseColorFactor[1]),
        static_cast<float>(pbr.baseColorFactor[2]),
        static_cast<float>(pbr.baseColorFactor[3])
    };
    outMaterial.metallicFactor = static_cast<float>(pbr.metallicFactor);
    outMaterial.roughnessFactor = static_cast<float>(pbr.roughnessFactor);

    // ベースカラーテクスチャ
    if (pbr.baseColorTexture.index >= 0) {
        outMaterial.hasBaseColorTexture = true;
        outMaterial.baseColorTexturePath = ResolveTexturePath(pbr.baseColorTexture.index);
        outMaterial.textureFilePath = outMaterial.baseColorTexturePath;
    }

    // メタリック・ラフネステクスチャ
    if (pbr.metallicRoughnessTexture.index >= 0) {
        outMaterial.hasMetallicRoughnessTexture = true;
        outMaterial.metallicRoughnessTexturePath = ResolveTexturePath(pbr.metallicRoughnessTexture.index);
    }

    // 法線マップ
    if (gltfMaterial.normalTexture.index >= 0) {
        outMaterial.hasNormalTexture = true;
        outMaterial.normalTexturePath = ResolveTexturePath(gltfMaterial.normalTexture.index);
        outMaterial.normalScale = static_cast<float>(gltfMaterial.normalTexture.scale);
    }

    // その他のプロパティ
    outMaterial.emissiveFactor = {
        static_cast<float>(gltfMaterial.emissiveFactor[0]),
        static_cast<float>(gltfMaterial.emissiveFactor[1]),
        static_cast<float>(gltfMaterial.emissiveFactor[2])
    };

    return true;
}

std::string TinyGLTFLoader::ResolveTexturePath(int textureIndex) {
    if (textureIndex < 0 || textureIndex >= gltfModel_.textures.size()) {
        return "";
    }

    const auto& texture = gltfModel_.textures[textureIndex];
    if (texture.source < 0 || texture.source >= gltfModel_.images.size()) {
        return "";
    }

    const auto& image = gltfModel_.images[texture.source];
    if (image.uri.empty()) {
        // 埋め込みテクスチャの場合は空パスを返す（Object3dでデフォルトテクスチャを使用）
        LogWarning("Embedded image detected (index: " + std::to_string(textureIndex) + "), will use default texture");
        return "";
    }

    return baseDirectory_ + image.uri;
}

Matrix4x4 TinyGLTFLoader::GetNodeTransform(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        // 行列が直接指定されている場合
        Matrix4x4 matrix;
        for (int i = 0; i < 16; ++i) {
            matrix.m[i / 4][i % 4] = static_cast<float>(node.matrix[i]);
        }
        return matrix;
    } else {
        // TRS (Translation, Rotation, Scale) から行列を構築
        Vector3 translation = {0.0f, 0.0f, 0.0f};
        if (node.translation.size() == 3) {
            translation = {
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2])
            };
        }

        Vector4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        if (node.rotation.size() == 4) {
            rotation = {
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2]),
                static_cast<float>(node.rotation[3])
            };
        }

        Vector3 scale = {1.0f, 1.0f, 1.0f};
        if (node.scale.size() == 3) {
            scale = {
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2])
            };
        }

        return MakeAffineMatrix(scale, rotation, translation);
    }
}

void TinyGLTFLoader::LogError(const std::string& message) {
    errorMessage_ = message;
    std::string logMessage = "TinyGLTFLoader ERROR: " + message + "\n";
    OutputDebugStringA(logMessage.c_str());
}

void TinyGLTFLoader::LogWarning(const std::string& message) {
    std::string logMessage = "TinyGLTFLoader WARNING: " + message + "\n";
    OutputDebugStringA(logMessage.c_str());
}