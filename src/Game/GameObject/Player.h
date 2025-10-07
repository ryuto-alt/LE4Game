#pragma once
#include "UnoEngine.h"

class Player {
public:
    Player();
    ~Player();

    void Initialize(Camera* camera, bool enableCollision = true, bool enableEnvironmentMap = false);
    void Update(UnoEngine* engine);
    void Draw();
    void DrawUI();
    void Finalize();
    
    // 位置・回転の取得・設定
    Vector3 GetPosition() const { return position_; }
    void SetPosition(const Vector3& position);
    float GetRotationY() const { return currentRotationY_; }
    
    // ライトの設定
    void SetDirectionalLight(const DirectionalLight& light);
    void SetSpotLight(const SpotLight& light);

    // 環境マップの有効/無効
    void EnableEnv(bool enable);
    bool IsEnvEnabled() const;
    void SetEnvTex(const std::string& texturePath);
    
    // アニメーション制御
    void PauseAnimation();
    void PlayAnimation();
    void ResetAnimation();
    void ToggleSneakWalk();
    bool IsAnimationPaused() const { return animationPaused_; }
    std::string GetCurrentAnimationName() const;
    
    // 状態取得
    bool IsMoving() const { return isMoving_; }
    bool IsSneaking() const { return isSneaking_; }
    bool IsBlending() const { return isBlending_; }
    float GetBlendProgress() const;
    
    // 回転スムーシング速度の設定
    void SetRotationSmoothingSpeed(float speed) { rotationSmoothingSpeed_ = speed; }
    float GetRotationSmoothingSpeed() const { return rotationSmoothingSpeed_; }
    float GetTargetRotationY() const { return targetRotationY_; }
    
    void HandleInput(UnoEngine* engine);
    void UpdateCameraSystem(UnoEngine* engine);
    void SetupCamera(UnoEngine* engine);

    // FPSカメラ用
    void UpdateFPSCamera(class FPSCamera* fpsCamera);
    void SetModelVisible(bool visible);

    // カメラ方向ベースの移動
    void MoveForward(float distance);
    void MoveRight(float distance);
    void MoveBackward(float distance);
    void MoveLeft(float distance);
    
    // 統合移動関数（複数キー同時押し対応）
    void MoveWithCameraDirection(float forward, float right, float deltaTime);
    
    // カメラフォロー機能
    void UpdateCameraFollow();

    // 移動停止の管理
    void StopMoving();

    // コリジョン用にObject3dを取得
    Object3d* GetObject() const { return object3d_.get(); }
    AnimatedModel* GetModel() const { return animatedModel_.get(); }

    // 衝突応答処理
    void HandleCollisionResponse();

    // 重力・ジャンプ関連
    void Jump();
    bool IsGrounded() const { return isGrounded_; }

private:
    void HandleMovement(UnoEngine* engine, float deltaTime);
    void HandleGamepadFeatures(UnoEngine* engine, float deltaTime);  // ゲームパッド固有機能
    void UpdateAnimation(float deltaTime);
    void UpdateRotation(UnoEngine* engine, float deltaTime);
    void UpdateGravity(float deltaTime);
    void CheckGroundCollision();
    
    // モデル関連
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<AnimatedModel> animatedModel_;
    
    // 位置・回転
    Vector3 position_ = Vector3{0.0f, 0.0f, 0.0f};
    Vector3 previousPosition_ = Vector3{0.0f, 0.0f, 0.0f};  // 前フレームの位置
    Vector3 smoothedPosition_ = Vector3{0.0f, 0.0f, 0.0f};  // カメラ用の滑らかな位置
    float currentRotationY_ = 0.0f;
    float targetRotationY_ = 0.0f;
    float rotationSmoothingSpeed_ = 17.0f;
    
    // 移動関連
    const float moveSpeed_ = 4.5f;
    const float sneakSpeedMultiplier_ = 0.5f;
    const float runSpeedMultiplier_ = 2.0f;
    bool isMoving_ = false;
    bool isSneaking_ = false;
    bool isRunning_ = false;
    bool previousBButtonPressed_ = false;
    Vector3 moveDirection_ = Vector3{0.0f, 0.0f, 0.0f};
    
    // アニメーション関連
    bool animationPaused_ = false;
    bool isBlending_ = false;
    float blendTimer_ = 0.0f;
    const float BLEND_DURATION = 0.3f;

    // 重力・ジャンプ関連
    Vector3 velocity_ = Vector3{0.0f, 0.0f, 0.0f};
    const float gravity_ = -25.0f;  // 重力を強化（より素早く落下）
    const float jumpPower_ = 10.0f;
    bool isGrounded_ = false;

    // カメラ参照
    Camera* camera_ = nullptr;
};