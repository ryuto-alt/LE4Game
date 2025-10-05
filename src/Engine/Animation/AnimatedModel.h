#pragma once
#include "Model.h"
#include "Animation.h"
#include "AnimationPlayer.h"
#include "AnimationBlender.h"
#include "AnimationUtility.h"
#include "TextureManager.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_map>

// ジョイント変換構造体
struct JointTransform {
    Vector3 scale;
    Quaternion rotate;
    Vector3 translate;
};

// アニメーション付きモデルクラス
class AnimatedModel : public Model {
public:
    // コンストラクタ
    AnimatedModel();
    
    // デストラクタ
    ~AnimatedModel();
    
    // 初期化
    void Initialize(DirectXCommon* dxCommon);
    
    // モデルとアニメーションの読み込み
    void LoadFromFile(const std::string& directoryPath, const std::string& filename);
    
    // GLTFファイルからの読み込み
    void LoadFromGLTF(const std::string& directoryPath, const std::string& filename);
    
    // アニメーションの読み込み
    void LoadAnimation(const std::string& directoryPath, const std::string& filename);
    
  
    void Update(float deltaTime);
    
    // アニメーションのローカル変換行列を取得
    Matrix4x4 GetAnimationLocalMatrix();
    
    // アニメーションプレイヤーを取得
    AnimationPlayer& GetAnimationPlayer() { return animationPlayer_; }
    const AnimationPlayer& GetAnimationPlayer() const { return animationPlayer_; }
    
    // アニメーションブレンダーを取得
    AnimationBlender& GetAnimationBlender() { return animationBlender_; }
    const AnimationBlender& GetAnimationBlender() const { return animationBlender_; }
    
    // スケルトンを取得
    Skeleton& GetSkeleton() { return skeleton_; }
    const Skeleton& GetSkeleton() const { return skeleton_; }
    
    // スキンクラスターを取得
    SkinCluster& GetSkinCluster() { return skinCluster_; }
    const SkinCluster& GetSkinCluster() const { return skinCluster_; }
    
    // アニメーション再生制御
    void PlayAnimation();
    void StopAnimation();
    void PauseAnimation();
    void SetAnimationLoop(bool loop);
    
    // アニメーションの追加
    void AddAnimation(const std::string& name, const Animation& animation);
    
    // アニメーションの切り替え（即座）
    void ChangeAnimation(const std::string& name);
    
    // アニメーションの切り替え（ブレンド遷移）
    void TransitionToAnimation(const std::string& name, float transitionDuration = 0.3f);
    
    // 現在のアニメーション名を取得
    const std::string& GetCurrentAnimationName() const { return currentAnimationName_; }
    
    // ブレンド中かどうか
    bool IsBlending() const { return isBlending_; }
    
    // ブレンド進行度を取得（0.0〜1.0）
    float GetBlendProgress() const { return blendProgress_; }
    
    // 指定したジョイントのブレンドされた変換を取得
    JointTransform GetBlendedTransform(const std::string& jointName, const JointTransform& originalTransform) const;
    
    // 初期ジョイント変換を取得
    const std::unordered_map<std::string, JointTransform>& GetInitialJointTransforms() const { return initialJointTransforms_; }

private:
    // assimpを使用したGLTFローダー
    void LoadFromGLTFWithAssimp(const std::string& directoryPath, const std::string& filename);
    
    // assimpシーンからモデルデータを作成
    void ProcessAssimpScene(const aiScene* scene, const std::string& directoryPath);
    
    // assimpメッシュからジオメトリデータを作成
    void ProcessAssimpMesh(const aiMesh* mesh, const aiScene* scene);
    
    // assimpマテリアルからマテリアルデータを作成
    void ProcessAssimpMaterial(const aiMaterial* material, const aiScene* scene, const std::string& directoryPath);
    
    // assimpノードからアニメーションデータを作成
    void ProcessAssimpAnimation(const aiScene* scene);
    
   
    Node ReadNode(aiNode* node);
    Skeleton CreateSkeleton(const Node& rootNode);
    int32_t CreateJoint(const Node& node, std::optional<int32_t> parent, std::vector<Joint>& joints);
    SkinCluster CreateSkinCluster();

    AnimationPlayer animationPlayer_;  // 現在のアニメーションプレイヤー
    AnimationPlayer targetPlayer_;     // ブレンド先のアニメーションプレイヤー
    AnimationBlender animationBlender_; // アニメーションブレンダー（使用しない）
    Animation animation_;              // アニメーション格納するでーた　
    std::string rootNodeName_;         // ルートノード名
    
    // 複数アニメーション管理
    std::unordered_map<std::string, Animation> animations_; // アニメーションの辞書
    std::string currentAnimationName_;  // 現在のアニメーション名
    std::string targetAnimationName_;   // ブレンド先のアニメーション名
    
    // ブレンド関連
    bool isBlending_ = false;          // ブレンド中かどうか
    float blendProgress_ = 0.0f;       // ブレンド進行度（0.0〜1.0）
    float blendDuration_ = 0.3f;       // ブレンドにかかる時間（秒）
    float blendElapsedTime_ = 0.0f;   // ブレンド開始からの経過時間
    
    // スキニング関連
    Skeleton skeleton_;                // スケルトン
    SkinCluster skinCluster_;          // スキンクラスター
    DirectXCommon* dxCommon_;          // DirectXCommon
    
    Assimp::Importer assimpImporter_;  // assimpインポーター
    
    // 初期ジョイント変換を保持
    std::unordered_map<std::string, JointTransform> initialJointTransforms_;
};