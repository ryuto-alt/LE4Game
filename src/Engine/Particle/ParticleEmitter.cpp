#include "ParticleEmitter.h"

ParticleEmitter::ParticleEmitter(
    const std::string& name,
    const Vector3& position,
    uint32_t emitCount,
    float emitRate,
    const Vector3& velocityMin,
    const Vector3& velocityMax,
    const Vector3& accelMin,
    const Vector3& accelMax,
    float startSizeMin,
    float startSizeMax,
    float endSizeMin,
    float endSizeMax,
    const Vector4& startColorMin,
    const Vector4& startColorMax,
    const Vector4& endColorMin,
    const Vector4& endColorMax,
    float rotationMin,
    float rotationMax,
    float rotationVelocityMin,
    float rotationVelocityMax,
    float lifeTimeMin,
    float lifeTimeMax)
    : name_(name),
    emitCount_(emitCount),
    emitRate_(emitRate),
    velocityMin_(velocityMin),
    velocityMax_(velocityMax),
    accelMin_(accelMin),
    accelMax_(accelMax),
    startSizeMin_(startSizeMin),
    startSizeMax_(startSizeMax),
    endSizeMin_(endSizeMin),
    endSizeMax_(endSizeMax),
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

    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = position;

    ParticleManager::GetInstance()->Emit(
        name_,
        transform_.translate,
        emitCount_ * 5,
        velocityMin_,
        velocityMax_,
        accelMin_,
        accelMax_,
        startSizeMin_,
        startSizeMax_,
        endSizeMin_,
        endSizeMax_,
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
}

void ParticleEmitter::Update() {
    if (!isEmitting_) {
        return;
    }

    currentTime_ += 1.0f / 60.0f;

    float interval = 1.0f / emitRate_;

    if (emitRate_ >= 100.0f) {
        if (!burstFired_) {
            ParticleManager::GetInstance()->Emit(
                name_,
                transform_.translate,
                emitCount_,
                velocityMin_,
                velocityMax_,
                accelMin_,
                accelMax_,
                startSizeMin_,
                startSizeMax_,
                endSizeMin_,
                endSizeMax_,
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
            isEmitting_ = false;
        }
    } else {
        if (currentTime_ >= interval) {
            ParticleManager::GetInstance()->Emit(
                name_,
                transform_.translate,
                emitCount_,
                velocityMin_,
                velocityMax_,
                accelMin_,
                accelMax_,
                startSizeMin_,
                startSizeMax_,
                endSizeMin_,
                endSizeMax_,
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