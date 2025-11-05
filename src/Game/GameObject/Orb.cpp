#include "Orb.h"
#include <cmath>

Orb::Orb() {
}

Orb::~Orb() {
}

void Orb::Initialize(const Vector3& position, Camera* camera) {
    camera_ = camera;
    position_ = position;
    basePosition_ = position;

    UnoEngine* engine = UnoEngine::GetInstance();

    OutputDebugStringA("Orb::Initialize - Starting orb initialization\n");

    // orbモデルの読み込み（gltfファイルなのでAnimatedModelとして読み込み）
    animatedModel_ = engine->CreateAnim();
    animatedModel_->LoadFromFile("Resources/Models/orb", "orb.gltf");

    OutputDebugStringA("Orb::Initialize - Model loaded\n");

    // Object3Dの作成と設定
    object3d_ = engine->CreateObj3();
    object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
    object3d_->SetAnimatedModel(animatedModel_.get());
    object3d_->SetPosition(position_);
    object3d_->SetScale(Vector3{1.0f, 1.0f, 1.0f});  // スケールを大きくして見やすく
    object3d_->SetRotation(rotation_);
    object3d_->SetEnableLighting(true);
    object3d_->SetCamera(camera_);
    object3d_->Update();  // 初期化時にも更新

    char debugMsg[256];
    sprintf_s(debugMsg, "Orb::Initialize - Position: (%.2f, %.2f, %.2f)\n", position_.x, position_.y, position_.z);
    OutputDebugStringA(debugMsg);
}

void Orb::Update(float deltaTime) {
    if (!isActive_) return;

    // 上下の浮遊アニメーション
    floatTimer_ += floatSpeed_ * deltaTime;
    float floatOffset = std::sin(floatTimer_) * floatAmplitude_;
    position_ = basePosition_;
    position_.y += floatOffset;

    // Object3Dに反映
    if (object3d_) {
        object3d_->SetPosition(position_);
        object3d_->SetRotation(rotation_);
        object3d_->Update();  // 重要：行列の更新
    }
}

void Orb::Draw() {
    if (!isActive_) {
        OutputDebugStringA("Orb::Draw - Not active, skipping draw\n");
        return;
    }

    if (object3d_) {
        OutputDebugStringA("Orb::Draw - Drawing orb\n");
        object3d_->Draw();
    } else {
        OutputDebugStringA("Orb::Draw - object3d_ is null!\n");
    }
}

void Orb::Finalize() {
    object3d_.reset();
    animatedModel_.reset();
}

void Orb::SetPosition(const Vector3& position) {
    position_ = position;
    basePosition_ = position;
    if (object3d_) {
        object3d_->SetPosition(position_);
    }
}

void Orb::SetDirectionalLight(const DirectionalLight& light) {
    if (object3d_) {
        object3d_->SetDirectionalLight(light);
    }
}

bool Orb::CheckCollisionWithPlayer(const Vector3& playerPos, float playerRadius) {
    if (!isActive_) return false;

    // 球体同士の衝突判定（距離ベース）
    Vector3 diff = Vector3{
        position_.x - playerPos.x,
        position_.y - playerPos.y,
        position_.z - playerPos.z
    };

    float distance = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    float combinedRadius = collisionRadius_ + playerRadius;

    return distance < combinedRadius;
}
