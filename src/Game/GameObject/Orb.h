#pragma once
#include "UnoEngine.h"

class Orb {
public:
    Orb();
    ~Orb();

    void Initialize(const Vector3& position, Camera* camera);
    void Update(float deltaTime);
    void Draw();
    void Finalize();

    // 位置・状態の取得
    Vector3 GetPosition() const { return position_; }
    void SetPosition(const Vector3& position);
    bool IsActive() const { return isActive_; }
    void SetActive(bool active) { isActive_ = active; }

    // ライトの設定
    void SetDirectionalLight(const DirectionalLight& light);
    void SetSpotLight(const SpotLight& light);

    // 衝突判定用
    Object3d* GetObject() const { return object3d_.get(); }
    float GetRadius() const { return collisionRadius_; }

    // プレイヤーとの当たり判定チェック
    bool CheckCollisionWithPlayer(const Vector3& playerPos, float playerRadius);

    // このOrbが取得されたかどうか
    bool IsCollected() const { return isCollected_; }
    void SetCollected(bool collected) { isCollected_ = collected; }

private:
    // スポットライトの照射判定
    void CheckSpotLightIllumination();
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<AnimatedModel> animatedModel_;

    Vector3 position_ = Vector3{0.0f, 0.0f, 0.0f};
    Vector3 rotation_ = Vector3{0.0f, 0.0f, 0.0f};
    bool isActive_ = true;

    // 回転アニメーション
    float rotationSpeed_ = 2.0f;

    // 上下の浮遊アニメーション
    float floatTimer_ = 0.0f;
    float floatSpeed_ = 2.0f;
    float floatAmplitude_ = 0.3f;
    Vector3 basePosition_ = Vector3{0.0f, 0.0f, 0.0f};

    // 衝突判定用半径（小さくしてより近づく必要がある）
    const float collisionRadius_ = 0.5f;

    Camera* camera_ = nullptr;

    // スポットライト照射判定用
    bool isIlluminatedBySpotLight_ = false;
    float glowIntensity_ = 0.0f;
    const SpotLight* currentSpotLight_ = nullptr;

    // このOrb単体の取得状態
    bool isCollected_ = false;
};
