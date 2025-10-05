#include "EffectManager3D.h"
#include <algorithm>
#include <sstream>
#include <random>
#include <cmath>

void EffectManager3D::Initialize() {
    // エフェクトプールの初期化
    hitEffectPool_.resize(POOL_SIZE);
    
    for (uint32_t i = 0; i < POOL_SIZE; ++i) {
        hitEffectPool_[i] = std::make_unique<HitEffect3D>();
        hitEffectPool_[i]->Initialize();
    }

    nextEffectIndex_ = 0;
}

void EffectManager3D::Finalize() {
    // すべてのエフェクトを停止
    StopAllEffects();
    
    // エフェクトプールのクリア
    hitEffectPool_.clear();
    
    nextEffectIndex_ = 0;
}

void EffectManager3D::Update() {
    // 全エフェクトの更新
    for (auto& effect : hitEffectPool_) {
        if (effect) {
            effect->Update();
        }
    }
}

void EffectManager3D::TriggerHitEffect(const Vector3& position, HitEffect3D::EffectType type) {
    // 使用可能なエフェクトを取得
    HitEffect3D* effect = GetAvailableEffect();
    
    if (effect) {
        // エフェクトを発生
        effect->TriggerHitEffect(position, type);
    }
}

void EffectManager3D::PlayNormalHit(const Vector3& position) {
    TriggerHitEffect(position, HitEffect3D::EffectType::Normal);
}

void EffectManager3D::PlayCriticalHit(const Vector3& position) {
    TriggerHitEffect(position, HitEffect3D::EffectType::Critical);
}

void EffectManager3D::PlayImpactHit(const Vector3& position) {
    TriggerHitEffect(position, HitEffect3D::EffectType::Impact);
}

void EffectManager3D::PlayExplosion(const Vector3& position) {
    TriggerHitEffect(position, HitEffect3D::EffectType::Explosion);
}

void EffectManager3D::PlayLightningHit(const Vector3& position) {
    TriggerHitEffect(position, HitEffect3D::EffectType::Lightning);
}

void EffectManager3D::StopAllEffects() {
    for (auto& effect : hitEffectPool_) {
        if (effect) {
            effect->Stop();
        }
    }
}

bool EffectManager3D::IsAnyEffectPlaying() const {
    for (const auto& effect : hitEffectPool_) {
        if (effect && effect->IsPlaying()) {
            return true;
        }
    }
    return false;
}

uint32_t EffectManager3D::GetActiveEffectCount() const {
    uint32_t count = 0;
    for (const auto& effect : hitEffectPool_) {
        if (effect && effect->IsPlaying()) {
            count++;
        }
    }
    return count;
}

std::string EffectManager3D::GetDebugInfo() const {
    std::ostringstream oss;
    oss << "EffectManager3D Debug Info:\n";
    oss << "Pool Size: " << POOL_SIZE << "\n";
    oss << "Active Effects: " << GetActiveEffectCount() << "\n";
    oss << "Next Index: " << nextEffectIndex_ << "\n";
    oss << "Any Playing: " << (IsAnyEffectPlaying() ? "Yes" : "No") << "\n";
    
    // 各エフェクトの状態
    for (uint32_t i = 0; i < hitEffectPool_.size(); ++i) {
        if (hitEffectPool_[i]) {
            oss << "Effect[" << i << "]: " 
                << (hitEffectPool_[i]->IsPlaying() ? "Playing" : "Stopped") << "\n";
        }
    }
    
    return oss.str();
}

HitEffect3D* EffectManager3D::GetAvailableEffect() {
    // まず停止中のエフェクトを探す
    for (auto& effect : hitEffectPool_) {
        if (effect && !effect->IsPlaying()) {
            return effect.get();
        }
    }

    // 停止中のエフェクトがない場合は、ラウンドロビン方式で選択
    HitEffect3D* effect = hitEffectPool_[nextEffectIndex_].get();
    
    // 既に再生中の場合は強制停止
    if (effect && effect->IsPlaying()) {
        effect->Stop();
    }

    // 次のインデックスに進める（循環）
    nextEffectIndex_ = (nextEffectIndex_ + 1) % POOL_SIZE;

    return effect;
}

