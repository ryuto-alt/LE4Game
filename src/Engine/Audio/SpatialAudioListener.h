#pragma once

#include "Mymath.h"

// 3D空間オーディオリスナークラス
class SpatialAudioListener {
public:
    // コンストラクタ
    SpatialAudioListener();
    ~SpatialAudioListener();

    // 位置・向きの設定
    void SetPosition(const Vector3& position);
    void SetVelocity(const Vector3& velocity);
    void SetOrientation(const Vector3& forward, const Vector3& up = Vector3{0.0f, 1.0f, 0.0f});

    // 位置・向きの取得
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetVelocity() const { return velocity_; }
    const Vector3& GetForward() const { return forward_; }
    const Vector3& GetUp() const { return up_; }

    // オーディオプロパティ
    void SetMasterVolume(float volume);
    void SetDopplerFactor(float factor);
    void SetSpeedOfSound(float speed);

    float GetMasterVolume() const { return masterVolume_; }
    float GetDopplerFactor() const { return dopplerFactor_; }
    float GetSpeedOfSound() const { return speedOfSound_; }

private:
    Vector3 position_;          // リスナーの位置
    Vector3 velocity_;          // リスナーの速度（ドップラー効果用）
    Vector3 forward_;           // リスナーの向き（前方向）
    Vector3 up_;               // リスナーの上方向

    float masterVolume_;        // マスター音量
    float dopplerFactor_;       // ドップラー効果の係数
    float speedOfSound_;        // 音速（m/s）
};