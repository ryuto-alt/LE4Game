#include "AnimatedModel.h"
#include "Mymath.h"
#include "UnoEngine.h"
#include <algorithm>
#include <cctype>
#include <fstream>

AnimatedModel::AnimatedModel() : rootNodeName_("root") {
}

AnimatedModel::~AnimatedModel() {
}

void AnimatedModel::Initialize(DirectXCommon* dxCommon) {
    Model::Initialize(dxCommon);
    dxCommon_ = dxCommon;
    animationBlender_.Initialize();
}

void AnimatedModel::LoadFromFile(const std::string& directoryPath, const std::string& filename) {
    // OutputDebugStringA(("AnimatedModel: Loading from " + directoryPath + "/" + filename + "\n").c_str());
    
    std::string extension = filename.substr(filename.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == "gltf") {
        LoadFromGLTFWithAssimp(directoryPath, filename);
    } else {
        LoadFromObj(directoryPath, filename);

        const ModelData& modelData = GetModelData();
        OutputDebugStringA(("AnimatedModel: Texture path: " + modelData.material.textureFilePath + "\n").c_str());

        ModelData& modelDataInternal2 = GetModelDataInternal();
        if (modelDataInternal2.material.textureFilePath.empty()) {
            OutputDebugStringA("AnimatedModel: No texture path found, setting default values\n");
            modelDataInternal2.material.textureFilePath = "Resources/uvChecker.png";
            modelDataInternal2.material.diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
            OutputDebugStringA(("AnimatedModel: Set default texture: " + modelDataInternal2.material.textureFilePath + "\n").c_str());
        }

        rootNodeName_ = "root";
    }

    // GLTFでもテクスチャが見つからない場合のデフォルト設定
    ModelData& modelDataInternal = GetModelDataInternal();
    if (modelDataInternal.material.textureFilePath.empty()) {
        OutputDebugStringA("AnimatedModel: No texture found after loading, setting default texture\n");
        modelDataInternal.material.textureFilePath = "Resources/uvChecker.png";
        modelDataInternal.material.diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
        TextureManager::GetInstance()->LoadTexture(modelDataInternal.material.textureFilePath);
        OutputDebugStringA(("AnimatedModel: Set default texture: " + modelDataInternal.material.textureFilePath + "\n").c_str());
    }

    // OutputDebugStringA(("AnimatedModel: Root node name set to: " + rootNodeName_ + "\n").c_str());

    if (extension != "gltf") {
        LoadAnimation(directoryPath, filename);
    }
}

void AnimatedModel::LoadFromGLTFWithAssimp(const std::string& directoryPath, const std::string& filename) {
    // OutputDebugStringA(("AnimatedModel: Loading GLTF with Assimp from " + directoryPath + "/" + filename + "\n").c_str());
    
    std::string fullPath = directoryPath + "/" + filename;
    
    const aiScene* scene = assimpImporter_.ReadFile(fullPath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        // OutputDebugStringA(("AnimatedModel: Error loading GLTF file: " + std::string(assimpImporter_.GetErrorString()) + "\n").c_str());
        return;
    }
    
    // OutputDebugStringA(("AnimatedModel: Successfully loaded GLTF file\n"));
    
    ProcessAssimpScene(scene, directoryPath);
    
    Node rootNode = ReadNode(scene->mRootNode);
    skeleton_ = CreateSkeleton(rootNode);
    skinCluster_ = CreateSkinCluster();
    
    for (const Joint& joint : skeleton_.joints) {
        JointTransform transform;
        transform.scale = joint.transform.scale;
        transform.rotate = joint.transform.rotate;
        transform.translate = joint.transform.translate;
        initialJointTransforms_[joint.name] = transform;
    }
    
    CreateVertexBuffer();
}

void AnimatedModel::LoadAnimation(const std::string& directoryPath, const std::string& filename) {
    animation_ = LoadAnimationFile(directoryPath, filename);
    
    animationPlayer_.SetAnimation(animation_);
    animationPlayer_.SetLoop(true);
    
    animationBlender_.SetAnimation(animation_);
    animationBlender_.SetLoop(true);
    animationBlender_.Play();
}

void AnimatedModel::Update(float deltaTime) {
    animationPlayer_.Update(deltaTime);
    
    if (isBlending_) {
        targetPlayer_.Update(deltaTime);
        
        blendElapsedTime_ += deltaTime;
        blendProgress_ = std::min(blendElapsedTime_ / blendDuration_, 1.0f);
        
        if (blendProgress_ >= 1.0f) {
            animationPlayer_ = targetPlayer_;
            currentAnimationName_ = targetAnimationName_;
            
            isBlending_ = false;
            blendProgress_ = 0.0f;
            blendElapsedTime_ = 0.0f;
        }
    }
    
    // animationBlender_.Update(deltaTime);
}

Matrix4x4 AnimatedModel::GetAnimationLocalMatrix() {
    // ブレンダーが有効な場合はブレンダーから取得
    if (!animations_.empty()) {
        return animationBlender_.GetLocalMatrix(rootNodeName_);
    }
    // 互換性のため、単一アニメーションの場合はプレイヤーから取得
    return animationPlayer_.GetLocalMatrix(rootNodeName_);
}

// アニメーション再生制御
void AnimatedModel::PlayAnimation() {
    animationPlayer_.Play();
    animationBlender_.Play();
}

void AnimatedModel::StopAnimation() {
    animationPlayer_.Stop();
    animationBlender_.Stop();
}

void AnimatedModel::PauseAnimation() {
    animationPlayer_.Pause();
    animationBlender_.Pause();
}

void AnimatedModel::SetAnimationLoop(bool loop) {
    animationPlayer_.SetLoop(loop);
    animationBlender_.SetLoop(loop);
}

// アニメーションの追加
void AnimatedModel::AddAnimation(const std::string& name, const Animation& animation) {
    animations_[name] = animation;
    
    // 最初のアニメーションの場合は自動的に設定
    if (animations_.size() == 1) {
        currentAnimationName_ = name;
        animationBlender_.SetAnimation(animation);
        animationBlender_.Play();
    }
}

// アニメーションの切り替え（即座）
void AnimatedModel::ChangeAnimation(const std::string& name) {
    auto it = animations_.find(name);
    if (it != animations_.end()) {
        currentAnimationName_ = name;
        animationPlayer_.SetAnimation(it->second);
        animationPlayer_.Play();
        // animationBlender_も更新（互換性のため）
        animationBlender_.SetAnimation(it->second);
        animationBlender_.Play();
    }
}

// アニメーションの切り替え（ブレンド遷移）
void AnimatedModel::TransitionToAnimation(const std::string& name, float transitionDuration) {
    auto it = animations_.find(name);
    if (it != animations_.end() && currentAnimationName_ != name) {
        // ブレンド開始
        targetAnimationName_ = name;
        targetPlayer_.SetAnimation(it->second);
        targetPlayer_.SetLoop(true); // 常にループ（後で改善）
        targetPlayer_.Play();
        targetPlayer_.SetTime(0.0f); // 新しいアニメーションは最初から
        
        isBlending_ = true;
        blendDuration_ = transitionDuration;
        blendElapsedTime_ = 0.0f;
        blendProgress_ = 0.0f;
    }
}


Node AnimatedModel::ReadNode(aiNode* node)
{
    Node result;
    aiVector3D scale, translate;
    aiQuaternion rotation;

    node->mTransformation.Decompose(scale, rotation, translate);
    result.transform.scale = { scale.x, scale.y, scale.z };
    result.transform.rotate = { rotation.x, -rotation.y, -rotation.z, rotation.w };  
    result.transform.translate = { -translate.x, translate.y, translate.z };  
    result.localMatrix = MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);
    result.name = node->mName.C_Str();
    result.children.resize(node->mNumChildren);
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }
    return result;
}

Skeleton AnimatedModel::CreateSkeleton(const Node& rootNode)
{
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }
    return skeleton;
}

int32_t AnimatedModel::CreateJoint(const Node& node, std::optional<int32_t> parent, std::vector<Joint>& joints)
{
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = MakeIdentity4x4();
    joint.transform.scale = node.transform.scale;
    joint.transform.rotate = node.transform.rotate;
    joint.transform.translate = node.transform.translate;
    joint.index = int32_t(joints.size());
    joint.parent = parent;

    joints.push_back(joint);

    for (const Node& child : node.children) {
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[joint.index].children.push_back(childIndex);
    }

    return joint.index;
}

SkinCluster AnimatedModel::CreateSkinCluster()
{
    SkinCluster skinCluster;
    
    // palette用のリソースを作成
    skinCluster.paletteResource = dxCommon_->CreateBufferResource(sizeof(WellForGPU) * skeleton_.joints.size());
    WellForGPU* mappedPalette = nullptr;
    skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
    skinCluster.mappedPalette = { mappedPalette, skeleton_.joints.size() };
    
    // SRVを作成
    // UnoEngineのSrvManagerを使用
    UnoEngine* engine = UnoEngine::GetInstance();
    if (engine && engine->GetSrvManager()) {
        SrvManager* srvManager = engine->GetSrvManager();
        uint32_t srvIndex = srvManager->Allocate();
        srvManager->CreateSRVForStructuredBuffer(srvIndex, skinCluster.paletteResource, 
                                                  static_cast<UINT>(skeleton_.joints.size()), sizeof(WellForGPU));
        skinCluster.paletteSrvHandle.first = srvManager->GetCPUDescriptorHandle(srvIndex);
        skinCluster.paletteSrvHandle.second = srvManager->GetGPUDescriptorHandle(srvIndex);
        
        // OutputDebugStringA(("AnimatedModel: Created palette SRV at index " + std::to_string(srvIndex) + "\n").c_str());
    } else {
        OutputDebugStringA("AnimatedModel: WARNING - Could not access SrvManager\n");
        skinCluster.paletteSrvHandle.first = {};
        skinCluster.paletteSrvHandle.second = {};
    }
    
    // Influence用のリソースを作成
    const ModelData& modelData = GetModelData();
    skinCluster.influenceResource = dxCommon_->CreateBufferResource(sizeof(VertexInfluence) * modelData.vertices.size());
    VertexInfluence* mappedInfluence = nullptr;
    skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
    std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size());
    skinCluster.mappedInfluence = { mappedInfluence, modelData.vertices.size() };
    
    // Influence用のVBVを作成
    skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
    skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
    skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);
    
    // InverseBindPoseMatrixを格納する場所を作成して、単位行列で埋める
    skinCluster.inverseBindPoseMatrices.resize(skeleton_.joints.size());
    std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), [] { return MakeIdentity4x4(); });
    
    // ボーンウェイト情報をインフルエンスデータに設定
    for (const auto& jointWeight : modelData.skinClusterData) {
        auto it = skeleton_.jointMap.find(jointWeight.first);
        if (it == skeleton_.jointMap.end()) {
            continue; // このジョイントがスケルトンに存在しない場合はスキップ
        }
        
        // InverseBindPoseMatrixを設定
        skinCluster.inverseBindPoseMatrices[it->second] = jointWeight.second.inverseBindPoseMatrix;
        
        // 各頂点のウェイト情報を設定
        for (const auto& vertexWeight : jointWeight.second.vertexWeights) {
            if (vertexWeight.vectorIndex >= modelData.vertices.size()) {
                continue; // 無効なインデックスはスキップ
            }
            
            auto& currentInfluence = skinCluster.mappedInfluence[vertexWeight.vectorIndex];
            
            // 空いているスロットにウェイトとジョイントインデックスを設定
            for (uint32_t index = 0; index < kNumMaxInfluence; ++index) {
                if (currentInfluence.weights[index] == 0.0f) {
                    currentInfluence.weights[index] = vertexWeight.weight;
                    currentInfluence.jointIndices[index] = it->second;
                    break;
                }
            }
        }
    }
    
    // ウェイトの正規化（合計が1になるように）
    for (size_t i = 0; i < modelData.vertices.size(); ++i) {
        auto& influence = skinCluster.mappedInfluence[i];
        float totalWeight = 0.0f;
        
        // 合計ウェイトを計算
        for (uint32_t j = 0; j < kNumMaxInfluence; ++j) {
            totalWeight += influence.weights[j];
        }
        
        // 正規化
        if (totalWeight > 0.0f) {
            for (uint32_t j = 0; j < kNumMaxInfluence; ++j) {
                influence.weights[j] /= totalWeight;
            }
        } else {
            // ウェイトが設定されていない頂点はルートジョイントに100%バインド
            influence.weights[0] = 1.0f;
            influence.jointIndices[0] = 0;
        }
    }
    
    // OutputDebugStringA(("AnimatedModel: Created SkinCluster with " + std::to_string(skeleton_.joints.size()) + " joints\n").c_str());
    
    return skinCluster;
}

// assimpシーンからモデルデータを作成
void AnimatedModel::ProcessAssimpScene(const aiScene* scene, const std::string& directoryPath) {
    ModelData& modelData = GetModelDataInternal();

    // 頂点データをクリア（複数メッシュを結合するため）
    modelData.vertices.clear();
    modelData.matVertexData.clear();
    modelData.materials.clear();

    OutputDebugStringA(("AnimatedModel: Loading " + std::to_string(scene->mNumMeshes) + " meshes\n").c_str());
    OutputDebugStringA(("AnimatedModel: Loading " + std::to_string(scene->mNumMaterials) + " materials\n").c_str());

    // すべてのマテリアルを先に読み込む
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        ProcessAssimpMaterial(scene->mMaterials[i], scene, directoryPath);
        OutputDebugStringA(("AnimatedModel: Material[" + std::to_string(i) + "] loaded\n").c_str());
    }

    // すべてのメッシュを処理（マルチマテリアル対応）
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        ProcessAssimpMesh(scene->mMeshes[i], scene);
    }

    OutputDebugStringA(("AnimatedModel: Total " + std::to_string(modelData.matVertexData.size()) + " mesh groups created\n").c_str());

    // ルートノードの設定
    if (scene->mRootNode) {
        rootNodeName_ = scene->mRootNode->mName.C_Str();
        // OutputDebugStringA(("AnimatedModel: Root node name: " + rootNodeName_ + "\n").c_str());
    }

    // アニメーションの処理
    ProcessAssimpAnimation(scene);
}

// assimpメッシュからジオメトリデータを作成
void AnimatedModel::ProcessAssimpMesh(const aiMesh* mesh, const aiScene* scene) {
    ModelData& modelData = GetModelDataInternal();

    // メッシュ名を取得（UTF-8からワイド文字列に変換）
    std::string utf8 = mesh->mName.C_Str();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring meshName(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &meshName[0], len);

    // マテリアルインデックスを取得
    size_t materialIndex = mesh->mMaterialIndex;

    OutputDebugStringA(("AnimatedModel: Processing mesh \"" + utf8 +
        "\" with material index " + std::to_string(materialIndex) +
        ", " + std::to_string(mesh->mNumVertices) + " vertices, " +
        std::to_string(mesh->mNumFaces) + " faces\n").c_str());

    // このメッシュ用のMaterialVertexDataを作成
    MaterialVertexData matVertexData;
    matVertexData.materialIndex = materialIndex;

    // まず頂点データを頂点インデックス順に格納（ボーンウェイトの参照用）
    std::vector<VertexData> indexedVertices(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        VertexData& vertex = indexedVertices[i];
        
        // 位置（右手座標系→左手座標系：X座標を反転）
        vertex.position = {
            -mesh->mVertices[i].x,  
            mesh->mVertices[i].y,
            mesh->mVertices[i].z,
            1.0f
        };
        
        // 法線（右手座標系→左手座標系：X成分を反転）
        if (mesh->HasNormals()) {
            vertex.normal = {
                -mesh->mNormals[i].x, 
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            };
        } else {
            vertex.normal = {0.0f, 1.0f, 0.0f};
        }
        
        // テクスチャ座標（UV1を優先、なければUV0を使用）
        if (mesh->GetNumUVChannels() > 1 && mesh->mTextureCoords[1]) {
            // TEXCOORD_1（第2UVセット）を使用
            vertex.texcoord = {
                mesh->mTextureCoords[1][i].x,
                mesh->mTextureCoords[1][i].y
            };
        } else if (mesh->mTextureCoords[0]) {
            // TEXCOORD_0（第1UVセット）を使用
            vertex.texcoord = {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            };
        } else {
            vertex.texcoord = {0.0f, 0.0f};
        }
    }
    
    // インデックスを使用して三角形ごとに頂点を作成（DirectX用に座標変換も適用）
    for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; faceIndex++) {
        const aiFace& face = mesh->mFaces[faceIndex];
        
        if (face.mNumIndices != 3) {
            // OutputDebugStringA(("AnimatedModel: Warning - Face " + std::to_string(faceIndex) + " has " + std::to_string(face.mNumIndices) + " indices (expected 3)\n").c_str());
            continue;
        }
        
        // X軸反転により巻き順を反転（0, 2, 1の順序）
        unsigned int indices[3] = { face.mIndices[0], face.mIndices[2], face.mIndices[1] };
        
        for (int i = 0; i < 3; i++) {
            unsigned int vertexIndex = indices[i];
            matVertexData.vertices.push_back(indexedVertices[vertexIndex]);
            modelData.vertices.push_back(indexedVertices[vertexIndex]);
        }
    }
    
    // ボーン情報の処理
    // OutputDebugStringA(("AnimatedModel: Processing " + std::to_string(mesh->mNumBones) + " bones\n").c_str());
    
    for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
        aiBone* bone = mesh->mBones[boneIndex];
        std::string jointName = bone->mName.C_Str();
        JointWeightData& jointWeightData = modelData.skinClusterData[jointName];
        
        // OutputDebugStringA(("AnimatedModel: Processing bone: " + jointName + " with " + std::to_string(bone->mNumWeights) + " weights\n").c_str());
        
        // InverseBindPose行列を取得（assimpのOffsetMatrixがInverseBindPose）
        aiMatrix4x4 offsetMatrix = bone->mOffsetMatrix;
        

        // InverseBindPose行列を分解して、各成分を変換してから再構築
        aiMatrix4x4 bindPoseMatrix = offsetMatrix.Inverse();
        aiVector3D scale, translate;
        aiQuaternion rotation;
        bindPoseMatrix.Decompose(scale, rotation, translate);
        
     
        Matrix4x4 bindPoseMatrixConverted = MakeAffineMatrix(
            { scale.x, scale.y, scale.z },                              // スケール
            { rotation.x, -rotation.y, -rotation.z, rotation.w },       // 回転（Y,Z成分を反転）
            { -translate.x, translate.y, translate.z }                  // 位置（X座標を反転）
        );
        
        // 逆バインドポーズ行列を格納
        jointWeightData.inverseBindPoseMatrix = Inverse(bindPoseMatrixConverted);
        
        // 頂点ウェイト情報を格納
        for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++) {
            const aiVertexWeight& weight = bone->mWeights[weightIndex];
            
            // 元の頂点インデックスから実際の頂点配列でのインデックスを探す
            // 各面で頂点が複製されているため、元のインデックスを持つすべての頂点を探す
            for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; faceIndex++) {
                const aiFace& face = mesh->mFaces[faceIndex];
                // X軸反転により巻き順を反転（0, 2, 1の順序）
                unsigned int indices[3] = { face.mIndices[0], face.mIndices[2], face.mIndices[1] };
                
                for (int i = 0; i < 3; i++) {
                    if (indices[i] == weight.mVertexId) {
                        // この頂点は modelData.vertices の (faceIndex * 3 + i) 番目に格納されている
                        VertexWeightData vwd;
                        vwd.weight = weight.mWeight;
                        vwd.vectorIndex = faceIndex * 3 + i;
                        jointWeightData.vertexWeights.push_back(vwd);
                    }
                }
            }
        }
    }

    // このメッシュのデータをmatVertexDataに格納
    modelData.matVertexData[meshName] = matVertexData;

    OutputDebugStringA(("AnimatedModel: Mesh \"" + utf8 + "\" created with " +
        std::to_string(matVertexData.vertices.size()) + " vertices\n").c_str());
}

// assimpマテリアルからマテリアルデータを作成
void AnimatedModel::ProcessAssimpMaterial(const aiMaterial* material, const aiScene* scene, const std::string& directoryPath) {
    ModelData& modelData = GetModelDataInternal();

    // 新しいマテリアルデータを作成
    MaterialData matData;

    // デフォルト値を設定
    matData.diffuse = {1.0f, 1.0f, 1.0f, 1.0f};
    matData.ambient = {0.2f, 0.2f, 0.2f, 1.0f};
    matData.specular = {0.5f, 0.5f, 0.5f, 1.0f};
    matData.alpha = 1.0f;

    // GLTFモデルはPBRマテリアルとして扱う
    matData.isPBR = true;
    matData.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    matData.metallicFactor = 0.0f;
    matData.roughnessFactor = 1.0f;
    
    // ディフューズテクスチャを取得
    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString texturePath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
            std::string textureFileName = texturePath.C_Str();

            // 埋め込みテクスチャのチェック (*0, *1 などで始まる)
            if (textureFileName[0] == '*') {
                // 埋め込みテクスチャのインデックスを取得
                int textureIndex = std::atoi(textureFileName.c_str() + 1);

                if (textureIndex >= 0 && textureIndex < static_cast<int>(scene->mNumTextures)) {
                    const aiTexture* embeddedTexture = scene->mTextures[textureIndex];

                    // 埋め込みテクスチャを保存
                    std::string savedTexturePath = directoryPath + "/embedded_texture_" + std::to_string(textureIndex);

                    if (embeddedTexture->mHeight == 0) {
                        // 圧縮形式（PNG, JPEGなど）
                        savedTexturePath += "." + std::string(embeddedTexture->achFormatHint);
                    } else {
                        // 非圧縮形式
                        savedTexturePath += ".png";
                    }

                    // ファイルに保存
                    std::ofstream outFile(savedTexturePath, std::ios::binary);
                    if (outFile.is_open()) {
                        if (embeddedTexture->mHeight == 0) {
                            // 圧縮データをそのまま書き込み
                            outFile.write(reinterpret_cast<const char*>(embeddedTexture->pcData), embeddedTexture->mWidth);
                        } else {
                            // 非圧縮データをPNGとして保存（STBを使用）
                            // 簡易実装: ここでは省略し、デフォルトテクスチャを使用
                            savedTexturePath = "";
                        }
                        outFile.close();
                        matData.textureFilePath = savedTexturePath;
                        OutputDebugStringA(("AnimatedModel: Saved embedded texture: " + savedTexturePath + "\n").c_str());
                    } else {
                        OutputDebugStringA("AnimatedModel: Failed to save embedded texture\n");
                        matData.textureFilePath = "";
                    }
                } else {
                    OutputDebugStringA("AnimatedModel: Invalid embedded texture index\n");
                    matData.textureFilePath = "";
                }
            } else {
                // 外部テクスチャファイル - URLデコードを適用
                // %20などのエンコードされた文字をデコード
                std::string decodedFileName = textureFileName;
                size_t pos = 0;
                while ((pos = decodedFileName.find('%', pos)) != std::string::npos) {
                    if (pos + 2 < decodedFileName.length()) {
                        std::string hex = decodedFileName.substr(pos + 1, 2);
                        try {
                            char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
                            decodedFileName.replace(pos, 3, 1, ch);
                        } catch (...) {
                            pos++;
                        }
                    } else {
                        break;
                    }
                }

                matData.textureFilePath = directoryPath + "/" + decodedFileName;

                // ファイルが存在するか確認
                DWORD fileAttributes = GetFileAttributesA(matData.textureFilePath.c_str());
                if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
                    // ファイルが見つからない場合、ファイル名のみを抽出して探索
                    std::string filenameOnly = decodedFileName;
                    size_t lastSlash = filenameOnly.find_last_of("/\\");
                    if (lastSlash != std::string::npos) {
                        filenameOnly = filenameOnly.substr(lastSlash + 1);
                    }

                    // 代替パスを試す
                    std::vector<std::string> possiblePaths = {
                        directoryPath + "/" + filenameOnly,
                        "Resources/textures/" + filenameOnly,
                        "Resources/" + filenameOnly
                    };

                    bool found = false;
                    for (const auto& path : possiblePaths) {
                        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                            matData.textureFilePath = path;
                            found = true;
                            OutputDebugStringA(("AnimatedModel: Found texture at: " + path + "\n").c_str());
                            break;
                        }
                    }

                    if (!found) {
                        OutputDebugStringA(("AnimatedModel: WARNING - Texture not found: " + matData.textureFilePath + "\n").c_str());
                        matData.textureFilePath = "";
                    }
                } else {
                    OutputDebugStringA(("AnimatedModel: Found texture: " + matData.textureFilePath + "\n").c_str());
                }

                // テクスチャを実際に読み込む
                if (!matData.textureFilePath.empty()) {
                    TextureManager::GetInstance()->LoadTexture(matData.textureFilePath);
                    OutputDebugStringA(("AnimatedModel: Loaded texture into TextureManager: " + matData.textureFilePath + "\n").c_str());
                }
            }
        }
    } else {
        // テクスチャなし
        matData.textureFilePath = "";
        // OutputDebugStringA("AnimatedModel: No texture found\n");
    }

    // マテリアルプロパティの取得
    aiColor3D color;
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        matData.diffuse = {color.r, color.g, color.b, 1.0f};
    }

    if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
        matData.ambient = {color.r, color.g, color.b, 1.0f};
    }

    if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
        matData.specular = {color.r, color.g, color.b, 1.0f};
    }

    float opacity;
    if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
        matData.alpha = opacity;
    }

    // materials配列に追加
    modelData.materials.push_back(matData);

    // 最初のマテリアルを単一マテリアルフィールドにも設定（下位互換性）
    if (modelData.materials.size() == 1) {
        modelData.material = matData;
    }

    OutputDebugStringA(("AnimatedModel: Material added - Texture: " + matData.textureFilePath + "\n").c_str());
}

// assimpノードからアニメーションデータを作成
void AnimatedModel::ProcessAssimpAnimation(const aiScene* scene) {
    if (scene->mNumAnimations == 0) {
        // OutputDebugStringA("AnimatedModel: No animations found in scene\n");
        return;
    }
    
    const aiAnimation* assimpAnimation = scene->mAnimations[0];
    
    // OutputDebugStringA(("AnimatedModel: Processing animation with " + std::to_string(assimpAnimation->mNumChannels) + " channels\n").c_str());
    
    // アニメーション時間を設定
    animation_.duration = static_cast<float>(assimpAnimation->mDuration / assimpAnimation->mTicksPerSecond);
    animation_.nodeAnimations.clear();
    
    // 各ノードアニメーションチャンネルを処理
    for (unsigned int i = 0; i < assimpAnimation->mNumChannels; i++) {
        const aiNodeAnim* nodeAnim = assimpAnimation->mChannels[i];
        std::string nodeName = nodeAnim->mNodeName.C_Str();
        
        NodeAnimation& nodeAnimation = animation_.nodeAnimations[nodeName];
        
        // 位置キーフレーム（右手座標系→左手座標系：X座標を反転）
        for (unsigned int j = 0; j < nodeAnim->mNumPositionKeys; j++) {
            const aiVectorKey& key = nodeAnim->mPositionKeys[j];
            KeyframeVector3 keyframe;
            keyframe.time = static_cast<float>(key.mTime / assimpAnimation->mTicksPerSecond);
            keyframe.value = {-key.mValue.x, key.mValue.y, key.mValue.z};  
            nodeAnimation.translate.push_back(keyframe);
        }
        
        // 回転キーフレーム（右手座標系→左手座標系）
        for (unsigned int j = 0; j < nodeAnim->mNumRotationKeys; j++) {
            const aiQuatKey& key = nodeAnim->mRotationKeys[j];
            KeyframeQuaternion keyframe;
            keyframe.time = static_cast<float>(key.mTime / assimpAnimation->mTicksPerSecond);
            

            // Y,Z成分を反転
            keyframe.value = {key.mValue.x, -key.mValue.y, -key.mValue.z, key.mValue.w};
            nodeAnimation.rotate.push_back(keyframe);
        }
        
        // スケールキーフレーム（スケールは座標系に依存しない）
        for (unsigned int j = 0; j < nodeAnim->mNumScalingKeys; j++) {
            const aiVectorKey& key = nodeAnim->mScalingKeys[j];
            KeyframeVector3 keyframe;
            keyframe.time = static_cast<float>(key.mTime / assimpAnimation->mTicksPerSecond);
            keyframe.value = {key.mValue.x, key.mValue.y, key.mValue.z};
            nodeAnimation.scale.push_back(keyframe);
        }
        
        // OutputDebugStringA(("AnimatedModel: Node " + nodeName + " - Position keys: " + std::to_string(nodeAnimation.translate.size()) + 
        //                   ", Rotation keys: " + std::to_string(nodeAnimation.rotate.size()) + 
        //                   ", Scale keys: " + std::to_string(nodeAnimation.scale.size()) + "\n").c_str());
    }
    
    animationPlayer_.SetAnimation(animation_);
    animationPlayer_.SetLoop(true);
    
    animationBlender_.SetAnimation(animation_);
    animationBlender_.SetLoop(true);
    animationBlender_.Play();
    
    // デフォルトアニメーションとして登録
    if (animations_.empty()) {
        animations_["default"] = animation_;
        currentAnimationName_ = "default";
    }
    
    // OutputDebugStringA(("AnimatedModel: Animation duration: " + std::to_string(animation_.duration) + " seconds\n").c_str());
}

// 指定したジョイントのブレンドされた変換を取得
JointTransform AnimatedModel::GetBlendedTransform(const std::string& jointName, const JointTransform& originalTransform) const {
    // 現在のアニメーションから変換を取得
    const Animation& currentAnim = animationPlayer_.GetAnimation();
    JointTransform currentTransform;
    
    if (auto it = currentAnim.nodeAnimations.find(jointName); it != currentAnim.nodeAnimations.end()) {
        const NodeAnimation& nodeAnimation = it->second;
        float currentTime = animationPlayer_.GetTime();
        
        // 各キーフレームリストが空でないことを確認
        if (!nodeAnimation.translate.empty()) {
            currentTransform.translate = CalculateValue(nodeAnimation.translate, currentTime);
        } else {
            currentTransform.translate = originalTransform.translate;
        }
        
        if (!nodeAnimation.rotate.empty()) {
            currentTransform.rotate = CalculateValue(nodeAnimation.rotate, currentTime);
        } else {
            currentTransform.rotate = originalTransform.rotate;
        }
        
        if (!nodeAnimation.scale.empty()) {
            currentTransform.scale = CalculateValue(nodeAnimation.scale, currentTime);
        } else {
                currentTransform.scale = originalTransform.scale;
        }
        
        // デバッグ: アニメーションからのスケール値を確認
        static int debugCount = 0;
        if (debugCount++ % 300 == 0) {
            OutputDebugStringA(("AnimatedModel::GetBlendedTransform - " + jointName + 
                               " has scale keys: " + (nodeAnimation.scale.empty() ? "NO" : "YES") +
                               ", using scale: (" + std::to_string(currentTransform.scale.x) + ", " +
                               std::to_string(currentTransform.scale.y) + ", " +
                               std::to_string(currentTransform.scale.z) + ")\n").c_str());
        }
    } else {
        currentTransform = originalTransform;
    }
    
    // ブレンド中でない場合は現在の変換をそのまま返す
    if (!isBlending_) {
        return currentTransform;
    }
    
    // ターゲットアニメーションから変換を取得
    const Animation& targetAnim = targetPlayer_.GetAnimation();
    JointTransform targetTransform;
    
    if (auto it = targetAnim.nodeAnimations.find(jointName); it != targetAnim.nodeAnimations.end()) {
        const NodeAnimation& nodeAnimation = it->second;
        float targetTime = targetPlayer_.GetTime();
        
        // 各キーフレームリストが空でないことを確認
        if (!nodeAnimation.translate.empty()) {
            targetTransform.translate = CalculateValue(nodeAnimation.translate, targetTime);
        } else {
            targetTransform.translate = originalTransform.translate;
        }
        
        if (!nodeAnimation.rotate.empty()) {
            targetTransform.rotate = CalculateValue(nodeAnimation.rotate, targetTime);
        } else {
            targetTransform.rotate = originalTransform.rotate;
        }
        
        if (!nodeAnimation.scale.empty()) {
            targetTransform.scale = CalculateValue(nodeAnimation.scale, targetTime);
        } else {
                targetTransform.scale = originalTransform.scale;
        }
    } else {
        targetTransform = originalTransform;
    }
    
    JointTransform blendedTransform;
    float t = blendProgress_;
    
    blendedTransform.scale = ::Lerp(currentTransform.scale, targetTransform.scale, t);
    
    blendedTransform.rotate = ::Slerp(currentTransform.rotate, targetTransform.rotate, t);
    
  
    blendedTransform.translate = ::Lerp(currentTransform.translate, targetTransform.translate, t);
    
    return blendedTransform;
}