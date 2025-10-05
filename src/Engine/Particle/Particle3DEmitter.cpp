#include "Particle3DEmitter.h"

Particle3DEmitter::Particle3DEmitter(
    const std::string& name,
    const Vector3& position,
    uint32_t emitCount,
    float emitRate,
    const Vector3& velocityMin,
    const Vector3& velocityMax,
    const Vector3& accelMin,
    const Vector3& accelMax,
    const Vector3& startScaleMin,
    const Vector3& startScaleMax,
    const Vector3& endScaleMin,
    const Vector3& endScaleMax,
    const Vector4& startColorMin,
    const Vector4& startColorMax,
    const Vector4& endColorMin,
    const Vector4& endColorMax,
    const Vector3& rotationMin,
    const Vector3& rotationMax,
    const Vector3& rotationVelocityMin,
    const Vector3& rotationVelocityMax,
    float lifeTimeMin,
    float lifeTimeMax)
    : name_(name),
    emitCount_(emitCount),
    emitRate_(emitRate),
    velocityMin_(velocityMin),
    velocityMax_(velocityMax),
    accelMin_(accelMin),
    accelMax_(accelMax),
    startScaleMin_(startScaleMin),
    startScaleMax_(startScaleMax),
    endScaleMin_(endScaleMin),
    endScaleMax_(endScaleMax),
    startColorMin_(startColorMin),
    startColorMax_(startColorMax),
    endColorMin_(endColorMin),
    endColorMax_(endColorMax),
    rotationMin_(rotationMin),
    rotationMax_(rotationMax),
    rotationVelocityMin_(rotationVelocityMin),
    rotationVelocityMax_(rotationVelocityMax),
    lifeTimeMin_(lifeTimeMin),
    lifeTimeMax_(lifeTimeMax) {

    // トランスフォームの初期化
    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = position;
}

void Particle3DEmitter::Update() {
    // 発生フラグがOFFなら処理しない
    if (!isEmitting_) {
        return;
    }

    // 時間を進める
    currentTime_ += 1.0f / 60.0f; // 60FPS想定

    // 発生頻度から発生タイミングを計算
    float interval = 1.0f / emitRate_;

    // 高い発生頻度（100.0f以上）の場合はバーストモード
    if (emitRate_ >= 100.0f) {
        // 一度だけ発生
        if (!burstFired_) {
            // 発生処理
            Particle3DManager::GetInstance()->Emit3D(
                name_,
                transform_.translate,
                emitCount_,
                velocityMin_,
                velocityMax_,
                accelMin_,
                accelMax_,
                startScaleMin_,
                startScaleMax_,
                endScaleMin_,
                endScaleMax_,
                startColorMin_,
                startColorMax_,
                endColorMin_,
                endColorMax_,
                rotationMin_,
                rotationMax_,
                rotationVelocityMin_,
                rotationVelocityMax_,
                lifeTimeMin_,
                lifeTimeMax_);

            burstFired_ = true;
            // バースト後は発生を停止
            isEmitting_ = false;
        }
    } else {
        // 通常モード：発生タイミングを超えていたらパーティクルを発生
        if (currentTime_ >= interval) {
            // 発生処理
            Particle3DManager::GetInstance()->Emit3D(
                name_,
                transform_.translate,
                emitCount_,
                velocityMin_,
                velocityMax_,
                accelMin_,
                accelMax_,
                startScaleMin_,
                startScaleMax_,
                endScaleMin_,
                endScaleMax_,
                startColorMin_,
                startColorMax_,
                endColorMin_,
                endColorMax_,
                rotationMin_,
                rotationMax_,
                rotationVelocityMin_,
                rotationVelocityMax_,
                lifeTimeMin_,
                lifeTimeMax_);

            // 経過時間を戻す（余剰分を考慮）
            currentTime_ -= interval;
        }
    }
}
