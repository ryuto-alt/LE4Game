#include "FPSCamera.h"

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
        engine->ResetMouseCenter();

        // マウスの移動量を取得
        float deltaX = 0.0f;
        float deltaY = 0.0f;
        engine->GetMouseMovement(deltaX, deltaY);

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
