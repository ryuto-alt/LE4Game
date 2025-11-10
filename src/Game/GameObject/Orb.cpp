#include "Orb.h"
#include "UnoEngine.h"
#include <cmath>

Orb::Orb() : object3d_(nullptr), animatedModel_(nullptr), position_(), rotation_(), isActive_(true), rotationSpeed_(2.0f), floatTimer_(0.0f), floatSpeed_(2.0f), floatAmplitude_(0.3f), basePosition_(), camera_(nullptr), isIlluminatedBySpotLight_(false), glowIntensity_(0.0f), currentSpotLight_(nullptr), isCollected_(false) {
}

Orb::~Orb() {
}

void Orb::Initialize(const Vector3& position, Camera* camera) {
    camera_ = camera;
    position_ = position;
    basePosition_ = position;
    isCollected_ = false;

    UnoEngine* engine = UnoEngine::GetInstance();

    OutputDebugStringA("Orb::Initialize - Starting orb initialization\n");

    // orbtest.gltf（単体モデル）を読み込み
    animatedModel_ = engine->CreateAnim();
    animatedModel_->LoadFromFile("Resources/Models/orb", "orbtest.gltf");

    OutputDebugStringA("Orb::Initialize - Model loaded\n");

    // Object3Dの作成と設定
    object3d_ = engine->CreateObj3();
    object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
    object3d_->SetAnimatedModel(animatedModel_.get());
    object3d_->SetPosition(position_);
    object3d_->SetScale(Vector3{1.0f, 1.0f, 1.0f});
    object3d_->SetRotation(rotation_);
    object3d_->SetEnableLighting(true);
    object3d_->SetCamera(camera_);
    // orbtestを真っ白に強く発光させる
    object3d_->SetEmissiveFactor(Vector3{20.0f, 20.0f, 20.0f }); // 白色の強い発光
    object3d_->Update();

    char debugMsg[256];
    sprintf_s(debugMsg, "Orb::Initialize - Position: (%.2f, %.2f, %.2f)\n", position_.x, position_.y, position_.z);
    OutputDebugStringA(debugMsg);
}

void Orb::Update(float deltaTime) {
    if (!isActive_ || isCollected_) return;

    // 上下の浮遊アニメーション
    floatTimer_ += floatSpeed_ * deltaTime;
    float floatOffset = std::sin(floatTimer_) * floatAmplitude_;
    position_ = basePosition_;
    position_.y += floatOffset;

    // スポットライトの照射状態を更新
    CheckSpotLightIllumination();

    // Object3Dに反映
    if (object3d_) {
        object3d_->SetPosition(position_);
        object3d_->SetRotation(rotation_);

        // スポットライトに当たっていたら発光効果を適用
        if (isIlluminatedBySpotLight_) {
            // 明るく光らせる（黄金色っぽく）
            float intensity = 1.0f + glowIntensity_ * 2.0f; // 光の強さに応じて明るさを増幅
            Vector4 glowColor = {
                1.0f * intensity,   // 赤
                0.95f * intensity,  // 緑
                0.6f * intensity,   // 青（黄金色）
                1.0f
            };
            object3d_->SetColor(glowColor);
        } else {
            // 照らされていない時は通常の色
            object3d_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
        }

        object3d_->Update();  // 重要：行列の更新
    }
}

void Orb::Draw() {
    if (!isActive_ || isCollected_) return;

    if (object3d_) {
        object3d_->Draw();
    }
}

void Orb::Finalize() {
    if (object3d_) {
        object3d_.reset();
    }
    if (animatedModel_) {
        animatedModel_.reset();
    }
}

void Orb::SetPosition(const Vector3& position) {
    position_ = position;
    basePosition_ = position;
    if (object3d_) {
        object3d_->SetPosition(position_);
        object3d_->Update();
    }
}

void Orb::SetDirectionalLight(const DirectionalLight& light) {
    if (object3d_) {
        object3d_->SetDirectionalLight(light);
    }
}

void Orb::SetSpotLight(const SpotLight& light) {
    currentSpotLight_ = &light;

    if (object3d_) {
        object3d_->SetSpotLight(light);
    }

    // スポットライトが当たっているかを判定
    CheckSpotLightIllumination();
}

void Orb::CheckSpotLightIllumination() {
    if (!currentSpotLight_ || !isActive_ || isCollected_) {
        isIlluminatedBySpotLight_ = false;
        glowIntensity_ = 0.0f;
        return;
    }

    // オーブからスポットライトの位置へのベクトル
    Vector3 toLight = {
        currentSpotLight_->position.x - position_.x,
        currentSpotLight_->position.y - position_.y,
        currentSpotLight_->position.z - position_.z
    };

    // 距離を計算
    float distance = sqrtf(toLight.x * toLight.x + toLight.y * toLight.y + toLight.z * toLight.z);

    // 正規化
    if (distance > 0.0001f) {
        toLight.x /= distance;
        toLight.y /= distance;
        toLight.z /= distance;
    }

    // スポットライトの方向ベクトルとの内積を計算（コーン角度チェック）
    float dotProduct = -(toLight.x * currentSpotLight_->direction.x +
                        toLight.y * currentSpotLight_->direction.y +
                        toLight.z * currentSpotLight_->direction.z);

    // スポットライトのコーン内にあるかチェック
    if (dotProduct > currentSpotLight_->outerCone) {
        // 減衰を計算
        float attenuation = 1.0f / (
            currentSpotLight_->attenuation.x +
            currentSpotLight_->attenuation.y * distance +
            currentSpotLight_->attenuation.z * distance * distance
        );

        // コーン減衰を計算
        float spotIntensity = 1.0f;
        if (dotProduct < currentSpotLight_->innerCone) {
            // 内側と外側のコーンの間での減衰
            float epsilon = currentSpotLight_->innerCone - currentSpotLight_->outerCone;
            if (epsilon > 0.0001f) {
                spotIntensity = (dotProduct - currentSpotLight_->outerCone) / epsilon;
                // クランプ処理（0.0f から 1.0f の範囲に制限）
                if (spotIntensity < 0.0f) spotIntensity = 0.0f;
                if (spotIntensity > 1.0f) spotIntensity = 1.0f;
            }
        }

        // 最終的な光の強度を計算
        glowIntensity_ = currentSpotLight_->intensity * attenuation * spotIntensity;
        isIlluminatedBySpotLight_ = (glowIntensity_ > 0.1f); // 閾値以上なら照らされていると判定
    } else {
        isIlluminatedBySpotLight_ = false;
        glowIntensity_ = 0.0f;
    }
}

bool Orb::CheckCollisionWithPlayer(const Vector3& playerPos, float playerRadius) {
    if (!isActive_ || isCollected_) return false;

    // 浮遊アニメーションを考慮した現在位置
    float floatOffset = std::sin(floatTimer_) * floatAmplitude_;
    Vector3 currentOrbPos = position_;
    currentOrbPos.y = basePosition_.y + floatOffset;

    // プレイヤーとの距離を計算
    float dx = currentOrbPos.x - playerPos.x;
    float dy = currentOrbPos.y - playerPos.y;
    float dz = currentOrbPos.z - playerPos.z;
    float distanceSq = dx * dx + dy * dy + dz * dz;

    // 衝突判定（半径の合計の二乗と比較）
    float collisionDistance = collisionRadius_ + playerRadius;
    if (distanceSq < collisionDistance * collisionDistance) {
        isCollected_ = true;

        char debugMsg[256];
        sprintf_s(debugMsg, "Orb collected at position: (%.2f, %.2f, %.2f)\n",
                  currentOrbPos.x, currentOrbPos.y, currentOrbPos.z);
        OutputDebugStringA(debugMsg);

        return true;
    }

    return false;
}