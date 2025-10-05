#pragma once

#include "ParticleManager.h"
#include "Mymath.h"
#include "Mymath.h"
#include <memory>

// パーティクルエミッタクラス
class ParticleEmitter {
public:
    // コンストラクタ
    ParticleEmitter(
        const std::string& name,
        const Vector3& position,
        uint32_t emitCount,
        float emitRate,
        const Vector3& velocityMin = { -1.0f, -1.0f, -1.0f },
        const Vector3& velocityMax = { 1.0f, 1.0f, 1.0f },
        const Vector3& accelMin = { 0.0f, 0.0f, 0.0f },
        const Vector3& accelMax = { 0.0f, -9.8f, 0.0f },
        float startSizeMin = 0.5f,
        float startSizeMax = 1.0f,
        float endSizeMin = 0.0f,
        float endSizeMax = 0.0f,
        const Vector4& startColorMin = { 1.0f, 1.0f, 1.0f, 1.0f },
        const Vector4& startColorMax = { 1.0f, 1.0f, 1.0f, 1.0f },
        const Vector4& endColorMin = { 1.0f, 1.0f, 1.0f, 0.0f },
        const Vector4& endColorMax = { 1.0f, 1.0f, 1.0f, 0.0f },
        float rotationMin = 0.0f,
        float rotationMax = 0.0f,
        float rotationVelocityMin = 0.0f,
        float rotationVelocityMax = 0.0f,
        float lifeTimeMin = 1.0f,
        float lifeTimeMax = 3.0f
    );

    // デストラクタ
    ~ParticleEmitter() = default;

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

    // 回転設定
    void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }

    // 回転取得
    const Vector3& GetRotation() const { return transform_.rotate; }

    // スケール設定
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    // スケール取得
    const Vector3& GetScale() const { return transform_.scale; }

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
    float startSizeMin_;
    float startSizeMax_;
    float endSizeMin_;
    float endSizeMax_;
    Vector4 startColorMin_;
    Vector4 startColorMax_;
    Vector4 endColorMin_;
    Vector4 endColorMax_;
    float rotationMin_;
    float rotationMax_;
    float rotationVelocityMin_;
    float rotationVelocityMax_;
    float lifeTimeMin_;
    float lifeTimeMax_;
};