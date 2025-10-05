#pragma once

#include "Particle3DManager.h"
#include "Mymath.h"
#include "Mymath.h"
#include <memory>

// 3Dパーティクルエミッタクラス
class Particle3DEmitter {
public:
    // コンストラクタ
    Particle3DEmitter(
        const std::string& name,
        const Vector3& position,
        uint32_t emitCount,
        float emitRate,
        const Vector3& velocityMin = { -1.0f, -1.0f, -1.0f },
        const Vector3& velocityMax = { 1.0f, 1.0f, 1.0f },
        const Vector3& accelMin = { 0.0f, 0.0f, 0.0f },
        const Vector3& accelMax = { 0.0f, -9.8f, 0.0f },
        const Vector3& startScaleMin = { 0.5f, 0.5f, 0.5f },
        const Vector3& startScaleMax = { 1.0f, 1.0f, 1.0f },
        const Vector3& endScaleMin = { 0.0f, 0.0f, 0.0f },
        const Vector3& endScaleMax = { 0.0f, 0.0f, 0.0f },
        const Vector4& startColorMin = { 1.0f, 1.0f, 1.0f, 1.0f },
        const Vector4& startColorMax = { 1.0f, 1.0f, 1.0f, 1.0f },
        const Vector4& endColorMin = { 1.0f, 1.0f, 1.0f, 0.0f },
        const Vector4& endColorMax = { 1.0f, 1.0f, 1.0f, 0.0f },
        const Vector3& rotationMin = { 0.0f, 0.0f, 0.0f },
        const Vector3& rotationMax = { 0.0f, 0.0f, 0.0f },
        const Vector3& rotationVelocityMin = { 0.0f, 0.0f, 0.0f },
        const Vector3& rotationVelocityMax = { 0.0f, 0.0f, 0.0f },
        float lifeTimeMin = 1.0f,
        float lifeTimeMax = 3.0f
    );

    // デストラクタ
    ~Particle3DEmitter() = default;

    // 更新
    void Update();

    // 発生フラグ設定
    void SetEmitting(bool isEmitting) { 
        isEmitting_ = isEmitting;
        if (isEmitting) {
            // 発生開始時にタイマーをリセット
            currentTime_ = 0.0f;
            burstFired_ = false; // バーストフラグをリセット
        }
    }

    // 発生フラグ取得
    bool IsEmitting() const { return isEmitting_; }

    // 座標設定
    void SetPosition(const Vector3& position) { transform_.translate = position; }

    // 座標取得
    const Vector3& GetPosition() const { return transform_.translate; }

    // Emit数設定
    void SetEmitCount(uint32_t emitCount) { emitCount_ = emitCount; }

    // Emit数取得
    uint32_t GetEmitCount() const { return emitCount_; }

    // Emit頻度設定
    void SetEmitRate(float emitRate) { emitRate_ = emitRate; }

    // Emit頻度取得
    float GetEmitRate() const { return emitRate_; }

private:
    // パーティクルグループ名
    std::string name_;

    // 発生フラグ
    bool isEmitting_ = true;

    // バーストフラグ（一度だけ発生するため）
    bool burstFired_ = false;

    // 座標・回転・スケール
    Transform transform_;

    // 経過時間
    float currentTime_ = 0.0f;

    // 発生するパーティクル数
    uint32_t emitCount_;

    // 発生頻度（秒間の発生回数）
    float emitRate_;

    // パーティクル設定
    Vector3 velocityMin_;
    Vector3 velocityMax_;
    Vector3 accelMin_;
    Vector3 accelMax_;
    Vector3 startScaleMin_;
    Vector3 startScaleMax_;
    Vector3 endScaleMin_;
    Vector3 endScaleMax_;
    Vector4 startColorMin_;
    Vector4 startColorMax_;
    Vector4 endColorMin_;
    Vector4 endColorMax_;
    Vector3 rotationMin_;
    Vector3 rotationMax_;
    Vector3 rotationVelocityMin_;
    Vector3 rotationVelocityMax_;
    float lifeTimeMin_;
    float lifeTimeMax_;
};
