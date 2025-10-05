#include "Enemy.h"
#ifdef _DEBUG
#include "imgui.h"
#endif
#include <cmath>

Enemy::Enemy() {
}

Enemy::~Enemy() {
}

void Enemy::Initialize(Camera* camera, const Vector3& position) {
    camera_ = camera;
    position_ = position;
    rotationY_ = 0.0f;

    // エンジンインスタンスの取得
    UnoEngine* engine = UnoEngine::GetInstance();

    // アニメーションモデルの作成と読み込み
    // LoadFromFileで全てのアニメーションが自動的に読み込まれる
    animatedModel_ = engine->CreateAnimatedModel();
    animatedModel_->LoadFromFile("Resources/Models/enemy/Enemy_Idle", "Enemy_Idle.gltf");

    OutputDebugStringA("Enemy: Model loaded, attempting to change animation\n");

    // アニメーション"Idle_Aggressive"に切り替え
    animatedModel_->ChangeAnimation("Idle_Aggressive");
    animatedModel_->PlayAnimation();
    animatedModel_->SetAnimationLoop(true);

    OutputDebugStringA("Enemy: Animation 'Idle_Aggressive' set and playing\n");

    // Object3Dの作成と設定
    object3d_ = engine->CreateObject3D();
    object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
    object3d_->SetAnimatedModel(animatedModel_.get());
    object3d_->SetPosition(position_);
    object3d_->SetScale(Vector3{1.0f, 1.0f, 1.0f});
    object3d_->SetRotation(Vector3{0.0f, rotationY_, 0.0f});
    object3d_->SetEnableLighting(true);
    object3d_->SetEnableAnimation(true);
    object3d_->SetCamera(camera_);

    // PBRマテリアルの設定
    if (animatedModel_) {
        MaterialData& material = const_cast<MaterialData&>(animatedModel_->GetMaterial());
        material.isPBR = true;
        material.baseColorFactor = { 0.8f, 0.3f, 0.3f, 1.0f };  // 敵らしい赤系の色
        material.metallicFactor = 0.1f;
        material.roughnessFactor = 0.7f;
        material.emissiveFactor = { 0.0f, 0.0f, 0.0f };
        material.alphaMode = "OPAQUE";
        material.doubleSided = false;
    }
}

void Enemy::Update(float deltaTime) {
    UpdateAnimation(deltaTime);

    // Object3Dの更新
    if (object3d_) {
        object3d_->SetPosition(position_);
        object3d_->SetRotation(Vector3{0.0f, rotationY_, 0.0f});
        object3d_->Update();
    }
}

void Enemy::Draw() {
    if (object3d_) {
        object3d_->Draw();
    }
}

void Enemy::Finalize() {
    object3d_.reset();
    animatedModel_.reset();
}

void Enemy::SetPosition(const Vector3& position) {
    position_ = position;
    if (object3d_) {
        object3d_->SetPosition(position_);
    }
}

void Enemy::SetRotation(float rotationY) {
    rotationY_ = rotationY;
    if (object3d_) {
        object3d_->SetRotation(Vector3{0.0f, rotationY_, 0.0f});
    }
}

void Enemy::SetDirectionalLight(const DirectionalLight& light) {
    if (object3d_) {
        object3d_->SetDirectionalLight(light);
    }
}

void Enemy::SetSpotLight(const SpotLight& light) {
    if (object3d_) {
        object3d_->SetSpotLight(light);
    }
}

void Enemy::PauseAnimation() {
    animationPaused_ = true;
    if (animatedModel_) {
        animatedModel_->PauseAnimation();
    }
}

void Enemy::PlayAnimation() {
    animationPaused_ = false;
    if (animatedModel_) {
        animatedModel_->PlayAnimation();
    }
}

void Enemy::ResetAnimation() {
    if (object3d_) {
        object3d_->SetAnimationTime(0.0f);
    }
}

void Enemy::UpdateAnimation(float deltaTime) {
    // Enemyは常にアニメーション再生
    if (animatedModel_) {
        // アニメーション速度を3倍に設定（デバッグ用）
        animatedModel_->Update(deltaTime * 3.0f);
    }
}

void Enemy::DrawImGui() {
#ifdef _DEBUG
    if (ImGui::TreeNode("Enemy")) {
        // 位置
        ImGui::DragFloat3("Position", &position_.x, 0.1f);
        if (object3d_) {
            object3d_->SetPosition(position_);
        }

        // 回転
        float rotationDeg = rotationY_ * (180.0f / 3.14159265359f);
        if (ImGui::DragFloat("Rotation Y (deg)", &rotationDeg, 1.0f)) {
            rotationY_ = rotationDeg * (3.14159265359f / 180.0f);
            if (object3d_) {
                object3d_->SetRotation(Vector3{0.0f, rotationY_, 0.0f});
            }
        }

        // アニメーション制御
        if (ImGui::CollapsingHeader("Animation")) {
            bool isPaused = animationPaused_;
            if (ImGui::Checkbox("Pause Animation", &isPaused)) {
                if (isPaused) {
                    PauseAnimation();
                } else {
                    PlayAnimation();
                }
            }

            if (ImGui::Button("Reset Animation")) {
                ResetAnimation();
            }

            // アニメーション選択
            if (animatedModel_) {
                static int selectedAnim = 10; // Idle_Aggressive
                const char* animNames[] = {
                    "0", "Action_Rolls", "Attack_Bite", "Attack_Bite.002",
                    "Attack_Hit", "Bake_Pose", "Default", "Die_1", "Die_2",
                    "Idel_Normal", "Idle_Aggressive", "Run-Cycle", "Walk-Cycle"
                };

                if (ImGui::Combo("Animation", &selectedAnim, animNames, IM_ARRAYSIZE(animNames))) {
                    animatedModel_->ChangeAnimation(animNames[selectedAnim]);
                    animatedModel_->PlayAnimation();
                }

                // アニメーション情報表示
                float animTime = object3d_ ? object3d_->GetAnimationTime() : 0.0f;
                ImGui::Text("Animation Time: %.3f", animTime);
            }
        }

        ImGui::TreePop();
    }
#endif
}
