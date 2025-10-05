#pragma once
#include "Model.h"
#include "Mymath.h"
#include "math.h"
#include "Camera.h"
#include "Animation.h"

#include <d3d12.h>
#include <wrl.h>
#include <memory>

class DirectXCommon;
class SpriteCommon;
class Camera;

// 3Dオブジェクトクラス
class Object3d {
public:
    // コンストラクタ
    Object3d();
    // デストラクタ
    ~Object3d();

    // 初期化
    void Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon);
    // モデルのセット
    void SetModel(Model* model);
    // 更新処理（従来のメソッド - 後方互換性のため残す）
    void Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);
    // 描画処理
    void Draw();

    // カメラ関連のメソッド
    void SetCamera(Camera* camera);
    Camera* GetCamera() const;

    // カメラを使用するUpdateメソッド
    void Update();

    // 座標の設定
    void SetPosition(const Vector3& position) { transform_.translate = position; }
    const Vector3& GetPosition() const { return transform_.translate; }

    // 回転の設定
    void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
    const Vector3& GetRotation() const { return transform_.rotate; }

    // スケールの設定
    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    const Vector3& GetScale() const { return transform_.scale; }

    // カラーの設定
    void SetColor(const Vector4& color) { materialData_->baseColorFactor = color; }
    const Vector4& GetColor() const { return materialData_->baseColorFactor; }

    // ライトを有効にするか
    void SetEnableLighting(bool enable) { materialData_->enableLighting = enable ? 1 : 0; }
    bool GetEnableLighting() const { return materialData_->enableLighting != 0; }

    // ライトの設定
    void SetDirectionalLight(const DirectionalLight& light) { *directionalLightData_ = light; }
    const DirectionalLight& GetDirectionalLight() const { return *directionalLightData_; }
    void SetSpotLight(const SpotLight& light) { *spotLightData_ = light; }
    const SpotLight& GetSpotLight() const { return *spotLightData_; }
    
    // 環境マップテクスチャの設定（全モデル共通・静的）
    static void SetEnvTex(const std::string& texturePath);
    static const std::string& GetEnvTex() { return globalEnvironmentTexturePath_; }

    // 環境マップの有効/無効（個別モデルごと）
    void EnableEnv(bool enable);
    bool IsEnvEnabled() const { return enableEnvironmentMap_; }
    
    // アニメーション行列の設定
    void SetAnimationMatrix(const Matrix4x4& animationMatrix) { animationMatrix_ = animationMatrix; }
    const Matrix4x4& GetAnimationMatrix() const { return animationMatrix_; }
    
    // スキニング関連メソッド
    void SkeletonUpdate(Skeleton& skeleton);
    void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);
    void SkinClusterUpdate(SkinCluster& skinCluster, const Skeleton& skeleton);
    
    // コリジョン設定
    void EnableCollision(bool enabled = true, const std::string& name = "");

    void SetAnimatedModel(class AnimatedModel* animatedModel);
    void SetEnableAnimation(bool enable);
    bool GetEnableAnimation() const;
    float GetAnimationTime() const;
    void SetAnimationTime(float time);

private:
    // モデル
    Model* model_;

    // DirectXCommon
    DirectXCommon* dxCommon_;
    // SpriteCommon
    SpriteCommon* spriteCommon_;

    // マテリアルリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    // マテリアルデータ
    Material* materialData_;

    // 変換行列リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    // 変換行列データ
    TransformationMatrix* transformationMatrixData_;

    // ライトリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    // ライトデータ
    DirectionalLight* directionalLightData_;
    
    // スポットライトリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    // スポットライトデータ
    SpotLight* spotLightData_;
    
    // カメラデータリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    // カメラデータ
    struct CameraData {
        Vector3 worldPosition;
    };
    CameraData* cameraData_;

    // トランスフォーム
    Transform transform_;
    
    // アニメーション行列
    Matrix4x4 animationMatrix_;
    
    // スキニング関連
    std::vector<Matrix4x4> skeletonPose_;
    

    float animationTime_ = 0.0f;
    bool enableAnimation_ = true;
    class AnimatedModel* animatedModel_ = nullptr;

    // カメラへの参照
    Camera* camera_ = nullptr;

    // 環境マップの有効/無効フラグ（個別）
    bool enableEnvironmentMap_ = false;

    // グローバル環境マップテクスチャパス（静的・全モデル共通）
    static std::string globalEnvironmentTexturePath_;
};