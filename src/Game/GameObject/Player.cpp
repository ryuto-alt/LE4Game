#include "Player.h"
#include "FPSCamera.h"
#include "../Engine/Resource/ResourcePreloader.h"
#include "../Engine/Collision/AABBCollision.h"
#ifdef _DEBUG
#include "imgui.h"
#endif
#include <cmath>

Player::Player() {
}

Player::~Player() {
}

void Player::Initialize(Camera* camera, bool enableCollision, bool enableEnvironmentMap) {
    camera_ = camera;

    // 初期位置と回転の設定
    position_ = Vector3{0.0f, 0.0f, 0.0f};
    smoothedPosition_ = position_;  // 初期化時は同じ位置
    currentRotationY_ = 0.0f;
    targetRotationY_ = 0.0f;
    
    // プリロードされたモデルの取得を試行
    UnoEngine* engine = UnoEngine::GetInstance();
    
    auto preloadedWalk = ResourcePreloader::GetInstance()->GetPreloadedModel("human_walk");
    if (preloadedWalk) {
        OutputDebugStringA("Player: Using preloaded walk model\n");
        animatedModel_ = std::move(preloadedWalk);
        
        // アニメーション登録
        Animation walkAnim = animatedModel_->GetAnimationPlayer().GetAnimation();
        animatedModel_->AddAnimation("walk", walkAnim);
        
        // スニークモデルもプリロード済みを使用
        auto preloadedSneak = ResourcePreloader::GetInstance()->GetPreloadedModel("human_sneak");
        if (preloadedSneak) {
            Animation sneakWalkAnim = preloadedSneak->GetAnimationPlayer().GetAnimation();
            animatedModel_->AddAnimation("sneakWalk", sneakWalkAnim);
            OutputDebugStringA("Player: Using preloaded sneak animation\n");
        } else {
            // フォールバック: 通常読み込み
            Animation sneakWalkAnim = engine->LoadAnimation("Resources/Models/human", "sneakWalk.gltf");
            animatedModel_->AddAnimation("sneakWalk", sneakWalkAnim);
        }
    } else {
        OutputDebugStringA("Player: Preloaded model not found, loading normally\n");
        // フォールバック: 通常読み込み
        animatedModel_ = engine->CreateAnimatedModel();
        animatedModel_->LoadFromFile("Resources/Models/human", "walk.gltf");
        
        Animation walkAnim = animatedModel_->GetAnimationPlayer().GetAnimation();
        animatedModel_->AddAnimation("walk", walkAnim);
        
        Animation sneakWalkAnim = engine->LoadAnimation("Resources/Models/human", "sneakWalk.gltf");
        animatedModel_->AddAnimation("sneakWalk", sneakWalkAnim);
    }
    
    animatedModel_->ChangeAnimation("walk");
    animatedModel_->PlayAnimation();
    
    // Object3Dの作成と設定
    object3d_ = engine->CreateObject3D();
    object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
    object3d_->SetAnimatedModel(animatedModel_.get());
    object3d_->SetPosition(position_);
    object3d_->SetScale(Vector3{1.0f, 1.0f, 1.0f});
    object3d_->SetRotation(Vector3{0.0f, 3.14f, 0.0f});
    object3d_->SetEnableLighting(true);
    object3d_->SetEnableAnimation(true);
    object3d_->SetCamera(camera_);
    
    // humanモデルのマテリアル情報を確認
    if (animatedModel_) {
        const MaterialData& material = animatedModel_->GetMaterial();
        char debugMsg[512];
        sprintf_s(debugMsg, "Player Human Model Material:\n"
            "  isPBR: %s\n"
            "  BaseColor: R=%.3f, G=%.3f, B=%.3f, A=%.3f\n"
            "  Metallic=%.3f, Roughness=%.3f\n"
            "  Texture: %s\n",
            material.isPBR ? "true" : "false",
            material.baseColorFactor.x, material.baseColorFactor.y,
            material.baseColorFactor.z, material.baseColorFactor.w,
            material.metallicFactor, material.roughnessFactor,
            material.textureFilePath.empty() ? "None" : material.textureFilePath.c_str());
        OutputDebugStringA(debugMsg);
        
        // PBRマテリアルでない場合、強制的にPBRを有効化
        if (!material.isPBR) {
            OutputDebugStringA("Player: Human model is not PBR, forcing PBR settings...\n");
            // モデルのマテリアルをPBRモードに強制変更
            MaterialData& mutableMaterial = const_cast<MaterialData&>(animatedModel_->GetMaterial());
            mutableMaterial.isPBR = true;
            mutableMaterial.baseColorFactor = { 0.8f, 0.7f, 0.6f, 1.0f };  // 人間っぽい肌色
            mutableMaterial.metallicFactor = 0.0f;   // 非金属
            mutableMaterial.roughnessFactor = 0.8f;  // 少し粗い表面
            mutableMaterial.emissiveFactor = { 0.0f, 0.0f, 0.0f };
            mutableMaterial.alphaMode = "OPAQUE";
            mutableMaterial.doubleSided = false;
            
            OutputDebugStringA("Player: Applied PBR material settings to human model\n");
            // SetModelを再呼び出しして更新されたマテリアルを適用
            object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
        }
        
        // 環境マップの設定
        if (enableEnvironmentMap) {
            Object3d::SetEnvTex("Resources/Models/skybox/rostock_laage_airport_4k.dds");
            object3d_->EnableEnv(true);
        } else {
            object3d_->EnableEnv(false);
            OutputDebugStringA("Player: Environment map is disabled\n");
        }
    }

    // コリジョン設定
    if (enableCollision && object3d_ && animatedModel_) {
        auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
        if (collisionManager) {
            Collision::AABB playerAABB = Collision::AABBExtractor::ExtractFromAnimatedModel(animatedModel_.get());
            collisionManager->RegisterObject(object3d_.get(), playerAABB, true, "Player");
        }
    }
}

void Player::Update(UnoEngine* engine) {

    const float deltaTime = engine->GetDeltaTime();

    // 移動前の位置を保存
    previousPosition_ = position_;

    // HandleMovement(engine, deltaTime);  // 新しい統合移動システムを使用するため一時的に無効化
    HandleGamepadFeatures(engine, deltaTime);  // ゲームパッド固有機能（スニーク切り替えなど）
    UpdateAnimation(deltaTime);
    UpdateRotation(engine, deltaTime);

    // 地面との衝突判定
    CheckGroundCollision();

    // 重力の適用
    UpdateGravity(deltaTime);

    // 速度を位置に適用
    position_.x += velocity_.x * deltaTime;
    position_.y += velocity_.y * deltaTime;
    position_.z += velocity_.z * deltaTime;

    object3d_->SetPosition(position_);
    object3d_->SetRotation(Vector3{0.0f, currentRotationY_, 0.0f});
    object3d_->Update();

    // 衝突応答処理
    HandleCollisionResponse();
}

// ゲームパッド固有機能処理（スニーク切り替えなど）
void Player::HandleGamepadFeatures(UnoEngine* engine, float deltaTime) {
    bool bButtonPressed = engine->IsXboxButtonPressed(0x2000);
    bool bButtonTriggered = bButtonPressed && !previousBButtonPressed_;
    
    // スニーク状態の切り替え（移動中のみ）
    if (bButtonTriggered && isMoving_ && !isBlending_) {
        isSneaking_ = !isSneaking_;
        isBlending_ = true;
        blendTimer_ = 0.0f;
        
        if (isSneaking_) {
            animatedModel_->TransitionToAnimation("sneakWalk", 0.3f);
        } else {
            animatedModel_->TransitionToAnimation("walk", 0.3f);
        }
    }
    
    previousBButtonPressed_ = bButtonPressed;
}

void Player::Draw() {
    if (!object3d_) return;
    object3d_->Draw();
}


void Player::DrawUI() {
#ifdef _DEBUG
    ImGui::Begin("Player");

    ImGui::Text("Controls:");
    ImGui::Separator();
    ImGui::Text("WASD - Move Player");
    ImGui::Text("P - Pause/Resume Animation");
    ImGui::Text("R - Reset Animation");
    ImGui::Text("1/RShift - Toggle SneakWalk");
    ImGui::Text("ESC - Exit Game");
    ImGui::Text("F1 - Toggle Free Camera");
    ImGui::Text("TAB - Toggle Mouse Look");

    ImGui::Separator();

    if (camera_) {
        Vector3 cameraPos = camera_->GetTranslate();
        ImGui::Text("Camera: (%.1f, %.1f, %.1f)", cameraPos.x, cameraPos.y, cameraPos.z);

        ImGui::Text("Camera Mode: %s", camera_->IsFreeCameraMode() ? "Free" : "Follow Player");
    }

    ImGui::Separator();
    ImGui::Text("Animation: %s", GetCurrentAnimationName().c_str());
    ImGui::Text("State: %s", IsMoving() ? "Moving" : "Idle");

    if (IsBlending()) {
        ImGui::Text("Blending: %.1f%%", GetBlendProgress() * 100.0f);
    }

    ImGui::Separator();
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", position_.x, position_.y, position_.z);
    ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", velocity_.x, velocity_.y, velocity_.z);
    ImGui::Text("Grounded: %s", isGrounded_ ? "YES" : "NO");

    float smoothingSpeed = GetRotationSmoothingSpeed();
    if (ImGui::SliderFloat("Smoothing Speed", &smoothingSpeed, 0.1f, 20.0f)) {
        SetRotationSmoothingSpeed(smoothingSpeed);
    }

    ImGui::End();
#endif
}

void Player::Finalize() {
    if (object3d_) {
        object3d_.reset();
    }
    if (animatedModel_) {
        animatedModel_.reset();
    }
}

void Player::SetDirectionalLight(const DirectionalLight& light) {
    if (object3d_) {
        object3d_->SetDirectionalLight(light);
    }
}

void Player::SetSpotLight(const SpotLight& light) {
    if (object3d_) {
        object3d_->SetSpotLight(light);
    }
}

void Player::SetEnvTex(const std::string& texturePath) {
    if (object3d_) {
        Object3d::SetEnvTex(texturePath);
    }
}

void Player::EnableEnv(bool enable) {
    if (object3d_) {
        object3d_->EnableEnv(enable);
    }
}

bool Player::IsEnvEnabled() const {
    if (object3d_) {
        return object3d_->IsEnvEnabled();
    }
    return false;
}

void Player::SetPosition(const Vector3& position) {
    position_ = position;
    if (object3d_) {
        object3d_->SetPosition(position_);
    }
}

void Player::PauseAnimation() {
    if (animatedModel_) {
        animationPaused_ = true;
        animatedModel_->PauseAnimation();
    }
}

void Player::PlayAnimation() {
    if (animatedModel_) {
        animationPaused_ = false;
        animatedModel_->PlayAnimation();
    }
}

void Player::ResetAnimation() {
    if (object3d_) {
        object3d_->SetAnimationTime(0.0f);
    }
}

void Player::ToggleSneakWalk() {
    if (!animatedModel_ || isBlending_) return;
    
    if (animatedModel_->GetCurrentAnimationName() == "walk") {
        animatedModel_->TransitionToAnimation("sneakWalk", 0.3f);
    } else {
        animatedModel_->TransitionToAnimation("walk", 0.3f);
    }
    
    isBlending_ = true;
    blendTimer_ = 0.0f;
}

std::string Player::GetCurrentAnimationName() const {
    if (!animatedModel_) return "";
    return animatedModel_->GetCurrentAnimationName();
}

float Player::GetBlendProgress() const {
    if (!isBlending_) return 1.0f;
    return blendTimer_ / BLEND_DURATION;
}

void Player::HandleMovement(UnoEngine* engine, float deltaTime) {
    float stickX = engine->GetXboxLeftStickX();
    float stickY = engine->GetXboxLeftStickY();
    bool bButtonPressed = engine->IsXboxButtonPressed(0x2000);
    bool bButtonTriggered = bButtonPressed && !previousBButtonPressed_;
    
    // 矢印キー入力の処理
    float keyboardInputX = 0.0f;
    float keyboardInputY = 0.0f;
    
    if (engine->IsKeyPressed(DIK_LEFTARROW)) {
        keyboardInputX = -1.0f;
    }
    if (engine->IsKeyPressed(DIK_RIGHTARROW)) {
        keyboardInputX = 1.0f;
    }
    if (engine->IsKeyPressed(DIK_UPARROW)) {
        keyboardInputY = 1.0f;
    }
    if (engine->IsKeyPressed(DIK_DOWNARROW)) {
        keyboardInputY = -1.0f;
    }
    
    // スティック入力と矢印キー入力を合成
    float totalX = stickX + keyboardInputX;
    float totalY = stickY + keyboardInputY;
    
    // 入力値を正規化（最大値1.0にクランプ）
    float totalMagnitude = std::sqrt(totalX * totalX + totalY * totalY);
    if (totalMagnitude > 1.0f) {
        totalX /= totalMagnitude;
        totalY /= totalMagnitude;
        totalMagnitude = 1.0f;
    }
    
    float stickMagnitude = std::sqrt(stickX * stickX + stickY * stickY);
    bool previouslyMoving = isMoving_;
    isMoving_ = totalMagnitude > 0.1f;
    
    // 移動開始時のアニメーション設定
    if (isMoving_ && !previouslyMoving && !isBlending_) {
        if (isSneaking_) {
            animatedModel_->TransitionToAnimation("sneakWalk", 0.3f);
        } else {
            animatedModel_->TransitionToAnimation("walk", 0.3f);
        }
    }
    
    // スニーク状態の切り替え
    if (bButtonTriggered && isMoving_ && !isBlending_) {
        isSneaking_ = !isSneaking_;
        isBlending_ = true;
        blendTimer_ = 0.0f;
        
        if (isSneaking_) {
            animatedModel_->TransitionToAnimation("sneakWalk", 0.3f);
        } else {
            animatedModel_->TransitionToAnimation("walk", 0.3f);
        }
    }
    
    previousBButtonPressed_ = bButtonPressed;
    
    // 移動処理（デルタタイム考慮）
    if (isMoving_) {
        float currentSpeed = isSneaking_ ? moveSpeed_ * sneakSpeedMultiplier_ : moveSpeed_;
        Vector3 movement = Vector3{totalX * currentSpeed * deltaTime, 0.0f, totalY * currentSpeed * deltaTime};
        
        moveDirection_ = Vector3{totalX, 0.0f, totalY};
        
        if (totalMagnitude > 0.1f) {
            targetRotationY_ = std::atan2(totalX, totalY);
        }
        
        position_ = position_ + movement;
    }
}

void Player::UpdateAnimation(float deltaTime) {
    // ブレンドタイマーの更新
    if (isBlending_) {
        blendTimer_ += deltaTime;
        
        if (blendTimer_ >= BLEND_DURATION) {
            isBlending_ = false;
            blendTimer_ = 0.0f;
        }
    }
    
    // 移動状態に応じたアニメーション制御
    if (isMoving_) {
        if (animationPaused_) {
            animationPaused_ = false;
            animatedModel_->PlayAnimation();
        }
    } else {
        if (!animationPaused_) {
            animationPaused_ = true;
            animatedModel_->PauseAnimation();
        }
    }
    
    // アニメーションの更新
    if (!animationPaused_) {
        animatedModel_->Update(deltaTime);
    } else {
        animatedModel_->Update(0.0f);
    }
}

void Player::UpdateRotation(UnoEngine* engine, float deltaTime) {
    currentRotationY_ = engine->SmoothRotation(currentRotationY_, targetRotationY_, rotationSmoothingSpeed_, deltaTime);
}

// カメラ方向ベースの移動機能
void Player::MoveForward(float distance) {
    if (!camera_) return;
    Vector3 forward = camera_->GetForwardVector();
    forward.y = 0.0f; // Y軸移動を無効化（水平移動のみ）
    float length = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    if (length > 0.0f) {
        forward.x /= length;
        forward.z /= length;
    }
    position_.x += forward.x * distance;
    position_.z += forward.z * distance;
    
    // 移動方向に応じて回転とアニメーション
    if (distance != 0.0f) {
        targetRotationY_ = std::atan2(forward.x, forward.z);
        
        // 移動状態とアニメーションの管理
        if (!isMoving_ && !isBlending_) {
            isMoving_ = true;
            if (isSneaking_) {
                animatedModel_->TransitionToAnimation("sneakWalk", 0.3f);
            } else {
                animatedModel_->TransitionToAnimation("walk", 0.3f);
            }
        }
    }
}

void Player::MoveBackward(float distance) {
    MoveForward(-distance);
}

void Player::MoveRight(float distance) {
    if (!camera_) return;
    Vector3 right = camera_->GetRightVector();
    right.y = 0.0f; // Y軸移動を無効化（水平移動のみ）
    float length = std::sqrt(right.x * right.x + right.z * right.z);
    if (length > 0.0f) {
        right.x /= length;
        right.z /= length;
    }
    position_.x += right.x * distance;
    position_.z += right.z * distance;
    
    // 移動方向に応じて回転とアニメーション
    if (distance != 0.0f) {
        targetRotationY_ = std::atan2(right.x, right.z);
        
        // 移動状態とアニメーションの管理
        if (!isMoving_ && !isBlending_) {
            isMoving_ = true;
            if (isSneaking_) {
                animatedModel_->TransitionToAnimation("sneakWalk", 0.3f);
            } else {
                animatedModel_->TransitionToAnimation("walk", 0.3f);
            }
        }
    }
}

void Player::MoveLeft(float distance) {
    MoveRight(-distance);
}

// 統合移動関数（複数キー同時押し対応）
void Player::MoveWithCameraDirection(float forward, float right, float deltaTime) {
    if (!camera_) return;
    
    // 入力がない場合は処理しない
    if (forward == 0.0f && right == 0.0f) {
        return;
    }
    
    // カメラの方向ベクトルを取得（水平方向のみ）
    Vector3 cameraForward = camera_->GetForwardVector();
    Vector3 cameraRight = camera_->GetRightVector();
    
    // Y成分を0にして水平移動のみに制限
    cameraForward.y = 0.0f;
    cameraRight.y = 0.0f;
    
    // 正規化
    float forwardLength = std::sqrt(cameraForward.x * cameraForward.x + cameraForward.z * cameraForward.z);
    float rightLength = std::sqrt(cameraRight.x * cameraRight.x + cameraRight.z * cameraRight.z);
    
    if (forwardLength > 0.0f) {
        cameraForward.x /= forwardLength;
        cameraForward.z /= forwardLength;
    }
    if (rightLength > 0.0f) {
        cameraRight.x /= rightLength;
        cameraRight.z /= rightLength;
    }
    
    // 最終的な移動方向を計算
    Vector3 moveDirection;
    moveDirection.x = cameraForward.x * forward + cameraRight.x * right;
    moveDirection.y = 0.0f;
    moveDirection.z = cameraForward.z * forward + cameraRight.z * right;
    
    // 移動方向を正規化
    float moveLength = std::sqrt(moveDirection.x * moveDirection.x + moveDirection.z * moveDirection.z);
    if (moveLength > 0.0f) {
        moveDirection.x /= moveLength;
        moveDirection.z /= moveLength;
        
        // 移動速度を適用
        float currentSpeed = moveSpeed_;
        if (isSneaking_) {
            currentSpeed *= sneakSpeedMultiplier_;
        } else if (isRunning_) {
            currentSpeed *= runSpeedMultiplier_;
        }
        float distance = currentSpeed * deltaTime;
        
        // プレイヤーの位置を更新
        position_.x += moveDirection.x * distance;
        position_.z += moveDirection.z * distance;
        
        // プレイヤーの向きを移動方向に設定
        targetRotationY_ = std::atan2(moveDirection.x, moveDirection.z);
        
        // 移動状態とアニメーションの管理
        if (!isMoving_ && !isBlending_) {
            isMoving_ = true;
            if (isSneaking_) {
                animatedModel_->TransitionToAnimation("sneakWalk", 0.3f);
                isBlending_ = true;
                blendTimer_ = 0.0f;
            } else {
                animatedModel_->TransitionToAnimation("walk", 0.3f);
                isBlending_ = true;
                blendTimer_ = 0.0f;
            }
        }
    }
}

// カメラフォロー機能（オービットカメラ）
void Player::UpdateCameraFollow() {
    if (!camera_) return;

    // プレイヤーの位置を滑らかに補間してカメラ用の位置を作成
    const float smoothingFactor = 0.15f;  // 小さいほど滑らか（0.1〜0.3推奨）
    smoothedPosition_.x = smoothedPosition_.x + (position_.x - smoothedPosition_.x) * smoothingFactor;
    smoothedPosition_.y = smoothedPosition_.y + (position_.y - smoothedPosition_.y) * smoothingFactor;
    smoothedPosition_.z = smoothedPosition_.z + (position_.z - smoothedPosition_.z) * smoothingFactor;

    // 滑らかな位置をオービットカメラのターゲットに設定
    camera_->SetOrbitTarget(smoothedPosition_);

    // オービットカメラの距離と高さを設定
    camera_->SetOrbitDistance(3.0f);  // プレイヤーから3ユニットの距離
    camera_->SetOrbitHeight(2.5f);    // プレイヤーから2.5ユニット上の高さ

    // オービットカメラの位置を更新
    camera_->UpdateOrbitCamera();
}

// 移動停止の管理
void Player::StopMoving() {
    if (isMoving_) {
        isMoving_ = false;
        // アニメーションを明示的に停止
        if (!animationPaused_) {
            animationPaused_ = true;
            if (animatedModel_) {
                animatedModel_->PauseAnimation();
            }
        }
    }
}


void Player::SetupCamera(UnoEngine* engine) {
    if (!camera_) return;
    
    camera_->SetFov(1.37f);
    engine->SetCameraStickSensitivity(2.5f);
    engine->SetCameraOrbitDistance(3.0f);
    engine->SetCameraOrbitHeight(2.5f);
    engine->SetCameraOrbitTarget(Vector3{0.0f, 0.0f, 0.0f});
}

void Player::HandleInput(UnoEngine* engine) {
    float deltaTime = engine->GetDeltaTime();

    if (engine->IsKeyTriggered(DIK_ESCAPE)) {
        engine->RequestEnd();
        return;
    }

#ifdef _DEBUG
    if (engine->IsKeyTriggered(DIK_TAB)) {
        camera_->ToggleMouseLook();
    }
    if (engine->IsKeyTriggered(DIK_F1)) {
        camera_->ToggleFreeCameraMode();
    }
#endif

    if (engine->IsKeyTriggered(DIK_P)) {
        if (IsAnimationPaused()) {
            PlayAnimation();
        } else {
            PauseAnimation();
        }
    }
    if (engine->IsKeyTriggered(DIK_R)) {
        ResetAnimation();
    }
    if (engine->IsKeyTriggered(DIK_1) || engine->IsKeyTriggered(DIK_RSHIFT)) {
        ToggleSneakWalk();
    }

    // ジャンプ処理
    if (engine->IsKeyTriggered(DIK_SPACE)) {
        char debugMsg[128];
        sprintf_s(debugMsg, "SPACE pressed! isGrounded: %s\n", isGrounded_ ? "true" : "false");
        OutputDebugStringA(debugMsg);

        if (isGrounded_) {
            Jump();
        }
    }

#ifdef _DEBUG
    if (camera_->IsFreeCameraMode()) {
        float cameraSpeed = 5.0f * deltaTime;
        if (engine->IsKeyPressed(DIK_W)) camera_->MoveForward(cameraSpeed);
        if (engine->IsKeyPressed(DIK_S)) camera_->MoveForward(-cameraSpeed);
        if (engine->IsKeyPressed(DIK_A)) camera_->MoveRight(-cameraSpeed);
        if (engine->IsKeyPressed(DIK_D)) camera_->MoveRight(cameraSpeed);
        if (engine->IsKeyPressed(DIK_SPACE)) camera_->MoveUp(cameraSpeed);
        if (engine->IsKeyPressed(DIK_LSHIFT)) camera_->MoveUp(-cameraSpeed);
    } else
#endif
    {
        isRunning_ = engine->IsKeyPressed(DIK_LSHIFT);

        float forward = 0.0f;
        float right = 0.0f;

        if (engine->IsKeyPressed(DIK_W)) forward += 1.0f;
        if (engine->IsKeyPressed(DIK_S)) forward -= 1.0f;
        if (engine->IsKeyPressed(DIK_A)) right -= 1.0f;
        if (engine->IsKeyPressed(DIK_D)) right += 1.0f;

        float stickX = engine->GetXboxLeftStickX();
        float stickY = engine->GetXboxLeftStickY();

        const float deadZone = 0.1f;
        if (abs(stickX) < deadZone) stickX = 0.0f;
        if (abs(stickY) < deadZone) stickY = 0.0f;

        forward += stickY;
        right += stickX;

        float totalMagnitude = std::sqrt(forward * forward + right * right);
        if (totalMagnitude > 1.0f) {
            forward /= totalMagnitude;
            right /= totalMagnitude;
        }

        bool isPlayerMoving = (forward != 0.0f || right != 0.0f);

        if (isPlayerMoving) {
            MoveWithCameraDirection(forward, right, deltaTime);
        } else if (IsMoving()) {
            StopMoving();
        }
    }
}

void Player::UpdateCameraSystem(UnoEngine* engine) {
    engine->UpdateCameraMouse();
    engine->UpdateCameraRightStick();

#ifdef _DEBUG
    if (!camera_->IsFreeCameraMode())
#endif
    {
        UpdateCameraFollow();
    }

    camera_->Update();
}

void Player::UpdateFPSCamera(FPSCamera* fpsCamera) {
    if (fpsCamera) {
        fpsCamera->UpdateCamera(camera_, position_, animatedModel_.get());
    }
}

void Player::SetModelVisible(bool visible) {
    // Object3dの描画フラグを管理する（仮実装）
    // この機能はObject3dクラスに描画フラグがあれば使用
    // ここでは何もしない（後でDrawメソッドで制御）
}

// 重力の更新
void Player::UpdateGravity(float deltaTime) {
    if (!isGrounded_) {
        velocity_.y += gravity_ * deltaTime;
    } else {
        velocity_.y = 0.0f;
    }
}

// 地面との衝突判定
void Player::CheckGroundCollision() {
    auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
    if (!collisionManager || !object3d_) {
        isGrounded_ = false;
        return;
    }

    auto playerColObj = collisionManager->FindCollisionObject(object3d_.get());
    if (!playerColObj || !playerColObj->IsEnabled()) {
        isGrounded_ = false;
        return;
    }

    // ジャンプ中（上昇中）は接地判定をスキップ
    if (velocity_.y > 0.1f) {
        isGrounded_ = false;
        return;
    }

    // すべてのコリジョンオブジェクトをチェック
    const auto& allObjects = collisionManager->GetCollisionObjects();
    isGrounded_ = false;

    for (const auto& otherObj : allObjects) {
        if (otherObj.get() == playerColObj.get() || !otherObj->IsEnabled()) {
            continue;
        }

        // すべてのオブジェクトで接地判定（Groundタグに限定しない）
        const Collision::AABB& playerAABB = playerColObj->GetWorldAABB();
        const Collision::AABB& objectAABB = otherObj->GetWorldAABB();

        // プレイヤーがオブジェクトの上にいるかチェック（XZ平面で重なっている）
        if (playerAABB.min.x < objectAABB.max.x && playerAABB.max.x > objectAABB.min.x &&
            playerAABB.min.z < objectAABB.max.z && playerAABB.max.z > objectAABB.min.z) {

            // プレイヤーの足元がオブジェクトの上端付近にあるかチェック
            float distanceToSurface = playerAABB.min.y - objectAABB.max.y;

            // 足元が表面付近にあり、かつ落下中または静止中の場合のみ接地
            if (distanceToSurface <= 0.5f && distanceToSurface >= -0.5f && velocity_.y <= 0.0f) {
                isGrounded_ = true;

                // 着地時の位置を滑らかに補間
                float targetY = objectAABB.max.y;
                float smoothFactor = 0.1f;  // 0.0〜1.0 (大きいほど速く補間)
                position_.y = position_.y + (targetY - position_.y) * smoothFactor;

                // 表面に十分近づいたら速度を0に
                if (std::abs(position_.y - targetY) < 0.01f) {
                    position_.y = targetY;
                    velocity_.y = 0.0f;
                }
                break;
            }
        }
    }
}

// ジャンプ
void Player::Jump() {
    if (isGrounded_) {
        velocity_.y = jumpPower_;
        isGrounded_ = false;

        char debugMsg[128];
        sprintf_s(debugMsg, "Jump executed! velocity_.y = %.2f\n", velocity_.y);
        OutputDebugStringA(debugMsg);
    } else {
        OutputDebugStringA("Jump failed: not grounded\n");
    }
}

// 衝突応答処理
void Player::HandleCollisionResponse() {
    auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
    if (!collisionManager || !object3d_) return;

    // プレイヤーのAABBを取得
    auto playerColObj = collisionManager->FindCollisionObject(object3d_.get());
    if (!playerColObj || !playerColObj->IsEnabled()) return;

    // 衝突解決の最大試行回数
    const int maxIterations = 3;

    for (int iteration = 0; iteration < maxIterations; ++iteration) {
        // 現在の位置でAABBを更新
        object3d_->SetPosition(position_);
        object3d_->Update();
        playerColObj->Update();

        const Collision::AABB& playerAABB = playerColObj->GetWorldAABB();

        bool hasCollision = false;
        Vector3 pushBack = {0.0f, 0.0f, 0.0f};

        // 全てのコリジョンオブジェクトをチェック
        const auto& allObjects = collisionManager->GetCollisionObjects();
        for (const auto& colObj : allObjects) {
            // 自分自身はスキップ
            if (colObj->GetObject() == object3d_.get()) continue;
            if (!colObj->IsEnabled()) continue;

            const Collision::AABB& otherAABB = colObj->GetWorldAABB();

            // 衝突チェック
            if (Collision::CheckAABBCollision(playerAABB, otherAABB)) {
                hasCollision = true;

                // 各軸での重なりを計算
                float overlapX = std::min(playerAABB.max.x - otherAABB.min.x,
                                          otherAABB.max.x - playerAABB.min.x);
                float overlapY = std::min(playerAABB.max.y - otherAABB.min.y,
                                          otherAABB.max.y - playerAABB.min.y);
                float overlapZ = std::min(playerAABB.max.z - otherAABB.min.z,
                                          otherAABB.max.z - playerAABB.min.z);

                // 小さなマージンを追加して完全に離す
                const float margin = 0.005f;

                // 最小の重なりを持つ軸で押し戻す（Y軸も含める）
                if (overlapX <= overlapY && overlapX <= overlapZ) {
                    // X軸方向の押し戻し
                    if (playerAABB.GetCenter().x < otherAABB.GetCenter().x) {
                        pushBack.x = -(overlapX + margin);
                    } else {
                        pushBack.x = (overlapX + margin);
                    }
                } else if (overlapY <= overlapX && overlapY <= overlapZ) {
                    // Y軸方向の押し戻し
                    if (playerAABB.GetCenter().y < otherAABB.GetCenter().y) {
                        pushBack.y = -(overlapY + margin);
                    } else {
                        pushBack.y = (overlapY + margin);
                    }
                } else {
                    // Z軸方向の押し戻し
                    if (playerAABB.GetCenter().z < otherAABB.GetCenter().z) {
                        pushBack.z = -(overlapZ + margin);
                    } else {
                        pushBack.z = (overlapZ + margin);
                    }
                }

                // 最初の衝突で押し戻しを適用して次のイテレーションへ
                break;
            }
        }

        // 衝突がなければ終了
        if (!hasCollision) {
            break;
        }

        // プレイヤーの位置を押し戻す
        position_.x += pushBack.x;
        position_.y += pushBack.y;
        position_.z += pushBack.z;
    }

    // 最終的な位置を更新
    object3d_->SetPosition(position_);
    object3d_->Update();
}
