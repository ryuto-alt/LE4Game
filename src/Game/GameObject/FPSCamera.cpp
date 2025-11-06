#include "FPSCamera.h"
#include <cmath>

FPSCamera::FPSCamera() {
}

FPSCamera::~FPSCamera() {
}

void FPSCamera::Initialize(bool enableFPS) {
    isFPSMode_ = enableFPS;
}

void FPSCamera::UpdateCamera(Camera* camera, const Vector3& playerPosition, AnimatedModel* model) {
    if (!camera) return;

    if (isFPSMode_) {
        // 一人称視点モード
        Vector3 targetPosition;

        if (model) {
            const Skeleton& skeleton = model->GetSkeleton();

            // 目のジョイントを取得
            auto it = skeleton.jointMap.find(eyeJointName_);
            if (it != skeleton.jointMap.end()) {
                int32_t jointIndex = it->second;
                const Joint& eyeJoint = skeleton.joints[jointIndex];

                // ワールド空間での目の位置を計算
                targetPosition.x = eyeJoint.skeletonSpaceMatrix.m[3][0] + playerPosition.x + eyeOffset_.x;
                targetPosition.y = eyeJoint.skeletonSpaceMatrix.m[3][1] + playerPosition.y + eyeOffset_.y;
                targetPosition.z = eyeJoint.skeletonSpaceMatrix.m[3][2] + playerPosition.z + eyeOffset_.z;
            } else {
                // ジョイントが見つからない場合はプレイヤー位置+オフセットを使用
                targetPosition = playerPosition;
                targetPosition.y += 1.6f; // デフォルトの目の高さ
            }
        } else {
            targetPosition = playerPosition;
            targetPosition.y += 1.6f;
        }

        // スムーシング処理（線形補間）
        Vector3 smoothedPosition;
        smoothedPosition.x = previousCameraPosition_.x + (targetPosition.x - previousCameraPosition_.x) * smoothFactor_;
        smoothedPosition.y = previousCameraPosition_.y + (targetPosition.y - previousCameraPosition_.y) * smoothFactor_;
        smoothedPosition.z = previousCameraPosition_.z + (targetPosition.z - previousCameraPosition_.z) * smoothFactor_;

        // カメラシェイクオフセットを適用
        smoothedPosition.x += cameraShakeOffset_.x;
        smoothedPosition.y += cameraShakeOffset_.y;
        smoothedPosition.z += cameraShakeOffset_.z;

        // カメラ位置を設定
        camera->SetTranslate(smoothedPosition);

        // 前回の位置を更新
        previousCameraPosition_ = smoothedPosition;

        // カメラの回転を適用
        camera->SetRotate(cameraRotation_);
    }
    // 三人称視点モードの場合は何もしない（既存のカメラシステムが処理）
}

void FPSCamera::UpdateCameraRotation(Camera* camera, UnoEngine* engine) {
    if (!camera || !engine || !isFPSMode_) return;

    // マウス視点が有効な場合はマウスを画面中央に固定
    if (mouseLookEnabled_) {
        engine->ResetMouse();

        // マウスの移動量を取得
        float deltaX = 0.0f;
        float deltaY = 0.0f;
        engine->GetMouseMove(deltaX, deltaY);

        // マウス移動量に基づいてカメラを回転
        cameraRotation_.y += deltaX * mouseSensitivity_; // ヨー（左右）
        cameraRotation_.x += deltaY * mouseSensitivity_; // ピッチ（上下）

        // ピッチの制限（真上・真下を向きすぎないように）
        const float maxPitch = 1.5f; // 約85度
        if (cameraRotation_.x > maxPitch) {
            cameraRotation_.x = maxPitch;
        }
        if (cameraRotation_.x < -maxPitch) {
            cameraRotation_.x = -maxPitch;
        }
    }
}

void FPSCamera::ToggleMouseLook() {
    mouseLookEnabled_ = !mouseLookEnabled_;

    UnoEngine* engine = UnoEngine::GetInstance();
    if (engine) {
        if (mouseLookEnabled_) {
            engine->SetCursor(false);
        } else {
            engine->SetCursor(true);
        }
    }
}

void FPSCamera::UpdateCameraShake(bool isMoving, bool isRunning, float deltaTime, UnoEngine* engine) {
    if (!isFPSMode_) {
        cameraShakeOffset_ = {0.0f, 0.0f, 0.0f};
        return;
    }

    // 足音の読み込み（初回のみ）
    if (!footSoundLoaded_ && engine) {
        engine->LoadAudio("footstep", "Resources/Audio/footsound.mp3");
        footSoundLoaded_ = true;
    }

    if (isMoving) {
        // 移動中の場合、カメラシェイクを適用
        float amplitude = isRunning ? runShakeAmplitude_ : walkShakeAmplitude_;
        float frequency = isRunning ? runShakeFrequency_ : walkShakeFrequency_;

        // タイマーをデルタタイムで進める
        shakeTimer_ += deltaTime;

        // sin波を使って上下の揺れを作成
        float yOffset = std::sin(shakeTimer_ * frequency) * amplitude;

        // 左右の揺れも追加（位相をずらす）
        float xOffset = std::sin(shakeTimer_ * frequency * 0.5f) * amplitude * 0.5f;

        // 足音を再生（揺れが下から上に切り替わるタイミング）
        if (engine && previousYOffset_ < 0.0f && yOffset >= 0.0f) {
            // 音が再生中でなければ再生
            if (!engine->IsAudPlay("footstep")) {
                engine->PlayAudio("footstep", false, 0.3f);  // ボリューム30%
            }
        }

        previousYOffset_ = yOffset;
        cameraShakeOffset_ = {xOffset, yOffset, 0.0f};
    } else {
        // 停止中の場合、揺れをスムーズに減衰（デルタタイム考慮）
        float decayFactor = std::pow(0.1f, deltaTime);  // 0.9fの60FPS相当
        cameraShakeOffset_.x *= decayFactor;
        cameraShakeOffset_.y *= decayFactor;
        cameraShakeOffset_.z *= decayFactor;

        // 十分小さくなったらリセット
        if (std::abs(cameraShakeOffset_.x) < 0.001f &&
            std::abs(cameraShakeOffset_.y) < 0.001f &&
            std::abs(cameraShakeOffset_.z) < 0.001f) {
            cameraShakeOffset_ = {0.0f, 0.0f, 0.0f};
            shakeTimer_ = 0.0f;
            previousYOffset_ = 0.0f;
        }
    }

    // 敵接近による恐怖シェイクを追加（横揺れのみ）
    if (fearShakeIntensity_ > 0.0f) {
        shakeTimer_ += deltaTime;

        // 小刻みで高周波な振動
        float fearFrequency = 25.0f;  // 高周波（小刻み）
        float fearAmplitude = 0.015f * fearShakeIntensity_;  // 強度に応じた振幅

        // 横方向（X軸）のみに小刻みに揺らす
        float fearX = std::sin(shakeTimer_ * fearFrequency) * fearAmplitude;
        float fearZ = std::sin(shakeTimer_ * fearFrequency * 0.7f) * fearAmplitude * 0.5f;

        // 既存のシェイクに加算（Yは加算しない）
        cameraShakeOffset_.x += fearX;
        cameraShakeOffset_.z += fearZ;
    }
}
