#pragma once
#include "UnoEngine.h"

class Enemy {
public:
    Enemy();
    ~Enemy();

    void Initialize(Camera* camera, const Vector3& position);
    void Update(float deltaTime);
    void Draw();
    void Finalize();

    // 位置・回転の取得・設定
    Vector3 GetPosition() const { return position_; }
    void SetPosition(const Vector3& position);
    float GetRotationY() const { return rotationY_; }
    void SetRotation(float rotationY);

    // ライトの設定
    void SetDirectionalLight(const DirectionalLight& light);
    void SetSpotLight(const SpotLight& light);

    // アニメーション制御
    void PauseAnimation();
    void PlayAnimation();
    void ResetAnimation();
    bool IsAnimationPaused() const { return animationPaused_; }

    // コリジョン用にObject3dを取得
    Object3d* GetObject() const { return object3d_.get(); }
    AnimatedModel* GetModel() const { return animatedModel_.get(); }

    // ImGui表示
    void DrawImGui();

private:
    void UpdateAnimation(float deltaTime);

    // モデル関連
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<AnimatedModel> animatedModel_;

    // 位置・回転
    Vector3 position_ = Vector3{0.0f, 0.0f, 0.0f};
    float rotationY_ = 0.0f;

    // アニメーション関連
    bool animationPaused_ = false;

    // カメラ参照
    Camera* camera_ = nullptr;
};
