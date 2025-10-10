#pragma once
#include "UnoEngine.h"
#include "../Engine/Animation/AnimatedModel.h"

class FPSCamera {
public:
    FPSCamera();
    ~FPSCamera();

    // 初期化
    void Initialize(bool enableFPS);

    // カメラモードの切り替え
    void SetFPSMode(bool enable) { isFPSMode_ = enable; }
    bool IsFPSMode() const { return isFPSMode_; }

    // カメラ更新
    void UpdateCamera(Camera* camera, const Vector3& playerPosition, AnimatedModel* model);

    // マウス入力でカメラを回転
    void UpdateCameraRotation(Camera* camera, UnoEngine* engine);

    // 目の位置のジョイント名を設定
    void SetEyeJointName(const std::string& jointName) { eyeJointName_ = jointName; }

    // カメラの回転を取得
    Vector3 GetCameraRotation() const { return cameraRotation_; }

    // マウス視点のON/OFF
    void SetMouseLookEnabled(bool enabled) { mouseLookEnabled_ = enabled; }
    bool IsMouseLookEnabled() const { return mouseLookEnabled_; }
    void ToggleMouseLook();

    // カメラシェイク
    void UpdateCameraShake(bool isMoving, bool isRunning, class UnoEngine* engine);
    Vector3 GetCameraShakeOffset() const { return cameraShakeOffset_; }

private:
    bool isFPSMode_ = false;           // true: 一人称, false: 三人称
    bool mouseLookEnabled_ = false;    // マウスによる視点移動の有効/無効（デフォルトはOFF）
    std::string eyeJointName_ = "Head"; // デフォルトは"Head"
    Vector3 eyeOffset_ = {0.0f, 0.15f, 0.0f}; // 目のオフセット（Headボーンからの相対位置）

    // カメラ回転
    Vector3 cameraRotation_ = {0.0f, 0.0f, 0.0f};
    float mouseSensitivity_ = 0.003f;

    // カメラ位置のスムーシング用
    Vector3 previousCameraPosition_ = {0.0f, 0.0f, 0.0f};
    float smoothFactor_ = 0.15f; // スムーシングの強さ (0.0-1.0, 小さいほど滑らか)

    // カメラシェイク用
    Vector3 cameraShakeOffset_ = {0.0f, 0.0f, 0.0f};
    float shakeTimer_ = 0.0f;
    float walkShakeAmplitude_ = 0.008f;  // 歩行時の揺れの大きさ
    float walkShakeFrequency_ = 6.0f;    // 歩行時の揺れの速さ
    float runShakeAmplitude_ = 0.025f;   // 走行時の揺れの大きさ
    float runShakeFrequency_ = 20.0f;     // 走行時の揺れの速さ

    // 足音用
    float previousYOffset_ = 0.0f;  // 前フレームのY方向オフセット
    bool footSoundLoaded_ = false;  // 足音が読み込まれたかどうか
};
