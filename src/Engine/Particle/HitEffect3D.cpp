#include "HitEffect3D.h"
#include "Particle3DManager.h"
#include <algorithm>

HitEffect3D::HitEffect3D() 
    : currentEffectType_(EffectType::Normal), isPlaying_(false) {
}

void HitEffect3D::Initialize() {
    // 3Dパーティクルグループの作成（effect.objを使用）
    Particle3DManager::GetInstance()->CreateParticle3DGroup("HitEffect", "effect.obj");

    // エフェクト設定の初期化
    InitializeEffectSettings();

    // エフェクトタイプ分のエミッターを作成
    emitters_.resize(static_cast<size_t>(EffectType::Lightning) + 1);
    
    for (int i = 0; i <= static_cast<int>(EffectType::Lightning); ++i) {
        EffectType type = static_cast<EffectType>(i);
        const auto& settings = effectSettings_[i];
        
        // エミッター名を作成
        std::string emitterName = "HitEffect_" + std::to_string(i);
        
        // エミッターを作成
        emitters_[i] = std::make_unique<Particle3DEmitter>(
            "HitEffect",  // パーティクルグループ名
            Vector3{0.0f, 0.0f, 0.0f},  // 初期位置
            settings.particleCount,
            settings.emitRate,
            settings.velocityMin,
            settings.velocityMax,
            settings.accelMin,
            settings.accelMax,
            settings.startScaleMin,
            settings.startScaleMax,
            settings.endScaleMin,
            settings.endScaleMax,
            settings.startColorMin,
            settings.startColorMax,
            settings.endColorMin,
            settings.endColorMax,
            Vector3{0.0f, 0.0f, 0.0f},  // 回転Min
            Vector3{0.0f, 0.0f, 0.0f},  // 回転Max
            settings.rotationVelocityMin,
            settings.rotationVelocityMax,
            settings.lifeTimeMin,
            settings.lifeTimeMax
        );
        
        // 初期状態では発生を停止
        emitters_[i]->SetEmitting(false);
    }
}

void HitEffect3D::Update() {
    // 全エミッターの更新
    for (auto& emitter : emitters_) {
        if (emitter) {
            emitter->Update();
        }
    }

    // エフェクトが再生中かチェック
    bool anyEmitting = false;
    for (auto& emitter : emitters_) {
        if (emitter && emitter->IsEmitting()) {
            anyEmitting = true;
            break;
        }
    }

    // 全てのエミッターが停止したら再生フラグをfalseに
    if (!anyEmitting) {
        isPlaying_ = false;
    }
}

void HitEffect3D::TriggerHitEffect(const Vector3& position, EffectType type) {
    // エフェクトタイプのインデックス
    int typeIndex = static_cast<int>(type);
    
    if (typeIndex < 0 || typeIndex >= emitters_.size()) {
        return;  // 無効なタイプ
    }

    // 現在のエフェクトタイプを設定
    currentEffectType_ = type;
    isPlaying_ = true;

    // 指定されたエフェクトタイプのエミッターを設定
    auto& emitter = emitters_[typeIndex];
    if (emitter) {
        // 位置を設定
        emitter->SetPosition(position);
        
        // エフェクト発生開始
        emitter->SetEmitting(true);
    }
}

bool HitEffect3D::IsPlaying() const {
    return isPlaying_;
}

void HitEffect3D::Stop() {
    // 全エミッターを停止
    for (auto& emitter : emitters_) {
        if (emitter) {
            emitter->SetEmitting(false);
        }
    }
    isPlaying_ = false;
}

void HitEffect3D::SetEffectSettings(EffectType type, const Vector3& velocityRange, 
                                   const Vector4& startColor, const Vector4& endColor,
                                   float lifeTime, uint32_t particleCount) {
    int typeIndex = static_cast<int>(type);
    
    if (typeIndex < 0 || typeIndex >= effectSettings_.size()) {
        return;  // 無効なタイプ
    }

    // 設定を更新
    auto& settings = effectSettings_[typeIndex];
    settings.velocityMin = Vector3{-velocityRange.x, -velocityRange.y, -velocityRange.z};
    settings.velocityMax = Vector3{velocityRange.x, velocityRange.y, velocityRange.z};
    settings.startColorMin = startColor;
    settings.startColorMax = startColor;
    settings.endColorMin = endColor;
    settings.endColorMax = endColor;
    settings.lifeTimeMin = lifeTime * 0.8f;  // 少し範囲を持たせる
    settings.lifeTimeMax = lifeTime * 1.2f;
    settings.particleCount = particleCount;
}

void HitEffect3D::InitializeEffectSettings() {
    effectSettings_.resize(static_cast<size_t>(EffectType::Lightning) + 1);

    // Normal（通常ヒット）
    {
        auto& settings = effectSettings_[static_cast<int>(EffectType::Normal)];
        settings.velocityMin = Vector3{-2.0f, -1.0f, -2.0f};
        settings.velocityMax = Vector3{2.0f, 3.0f, 2.0f};
        settings.accelMin = Vector3{0.0f, -9.8f, 0.0f};
        settings.accelMax = Vector3{0.0f, -9.8f, 0.0f};
        settings.startScaleMin = Vector3{0.1f, 0.1f, 0.1f};     // 小さく開始
        settings.startScaleMax = Vector3{0.2f, 0.2f, 0.2f};     // 小さく開始
        settings.endScaleMin = Vector3{0.3f, 0.3f, 0.3f};
        settings.endScaleMax = Vector3{0.6f, 0.6f, 0.6f};
        settings.startColorMin = Vector4{1.0f, 0.8f, 0.0f, 0.5f};  // 半透明から開始
        settings.startColorMax = Vector4{1.0f, 1.0f, 0.2f, 0.5f};
        settings.endColorMin = Vector4{1.0f, 0.3f, 0.0f, 0.0f};    // オレンジでフェードアウト
        settings.endColorMax = Vector4{1.0f, 0.5f, 0.0f, 0.0f};
        settings.rotationVelocityMin = Vector3{-3.14f, -3.14f, -3.14f};
        settings.rotationVelocityMax = Vector3{3.14f, 3.14f, 3.14f};
        settings.lifeTimeMin = 0.3f;
        settings.lifeTimeMax = 0.8f;
        settings.particleCount = 8;
        settings.emitRate = 100.0f;  // バーストモード
    }

    // Critical（クリティカルヒット）
    {
        auto& settings = effectSettings_[static_cast<int>(EffectType::Critical)];
        settings.velocityMin = Vector3{-4.0f, -2.0f, -4.0f};
        settings.velocityMax = Vector3{4.0f, 6.0f, 4.0f};
        settings.accelMin = Vector3{0.0f, -12.0f, 0.0f};
        settings.accelMax = Vector3{0.0f, -12.0f, 0.0f};
        settings.startScaleMin = Vector3{0.5f, 0.5f, 0.5f};
        settings.startScaleMax = Vector3{1.0f, 1.0f, 1.0f};
        settings.endScaleMin = Vector3{0.1f, 0.1f, 0.1f};
        settings.endScaleMax = Vector3{0.3f, 0.3f, 0.3f};
        settings.startColorMin = Vector4{1.0f, 0.2f, 0.2f, 1.0f};  // 赤色
        settings.startColorMax = Vector4{1.0f, 0.4f, 0.4f, 1.0f};
        settings.endColorMin = Vector4{0.8f, 0.0f, 0.0f, 0.0f};    // 深い赤でフェードアウト
        settings.endColorMax = Vector4{1.0f, 0.1f, 0.1f, 0.0f};
        settings.rotationVelocityMin = Vector3{-6.28f, -6.28f, -6.28f};
        settings.rotationVelocityMax = Vector3{6.28f, 6.28f, 6.28f};
        settings.lifeTimeMin = 0.5f;
        settings.lifeTimeMax = 1.2f;
        settings.particleCount = 15;
        settings.emitRate = 100.0f;  // バーストモード
    }

    // Impact（衝撃）
    {
        auto& settings = effectSettings_[static_cast<int>(EffectType::Impact)];
        settings.velocityMin = Vector3{-6.0f, -3.0f, -6.0f};
        settings.velocityMax = Vector3{6.0f, 8.0f, 6.0f};
        settings.accelMin = Vector3{0.0f, -15.0f, 0.0f};
        settings.accelMax = Vector3{0.0f, -15.0f, 0.0f};
        settings.startScaleMin = Vector3{0.4f, 0.4f, 0.4f};
        settings.startScaleMax = Vector3{0.8f, 0.8f, 0.8f};
        settings.endScaleMin = Vector3{0.2f, 0.2f, 0.2f};
        settings.endScaleMax = Vector3{0.4f, 0.4f, 0.4f};
        settings.startColorMin = Vector4{0.2f, 0.4f, 1.0f, 1.0f};  // 青色
        settings.startColorMax = Vector4{0.4f, 0.6f, 1.0f, 1.0f};
        settings.endColorMin = Vector4{0.0f, 0.2f, 0.8f, 0.0f};    // 深い青でフェードアウト
        settings.endColorMax = Vector4{0.1f, 0.3f, 1.0f, 0.0f};
        settings.rotationVelocityMin = Vector3{-9.42f, -9.42f, -9.42f};
        settings.rotationVelocityMax = Vector3{9.42f, 9.42f, 9.42f};
        settings.lifeTimeMin = 0.4f;
        settings.lifeTimeMax = 1.0f;
        settings.particleCount = 12;
        settings.emitRate = 100.0f;  // バーストモード
    }

    // Explosion（爆発）
    {
        auto& settings = effectSettings_[static_cast<int>(EffectType::Explosion)];
        settings.velocityMin = Vector3{-8.0f, -4.0f, -8.0f};
        settings.velocityMax = Vector3{8.0f, 10.0f, 8.0f};
        settings.accelMin = Vector3{0.0f, -20.0f, 0.0f};
        settings.accelMax = Vector3{0.0f, -20.0f, 0.0f};
        settings.startScaleMin = Vector3{0.8f, 0.8f, 0.8f};
        settings.startScaleMax = Vector3{1.5f, 1.5f, 1.5f};
        settings.endScaleMin = Vector3{0.1f, 0.1f, 0.1f};
        settings.endScaleMax = Vector3{0.5f, 0.5f, 0.5f};
        settings.startColorMin = Vector4{1.0f, 0.5f, 0.0f, 1.0f};  // オレンジ色
        settings.startColorMax = Vector4{1.0f, 0.7f, 0.2f, 1.0f};
        settings.endColorMin = Vector4{0.5f, 0.0f, 0.0f, 0.0f};    // 暗い赤でフェードアウト
        settings.endColorMax = Vector4{0.8f, 0.2f, 0.0f, 0.0f};
        settings.rotationVelocityMin = Vector3{-12.56f, -12.56f, -12.56f};
        settings.rotationVelocityMax = Vector3{12.56f, 12.56f, 12.56f};
        settings.lifeTimeMin = 0.8f;
        settings.lifeTimeMax = 1.8f;
        settings.particleCount = 25;
        settings.emitRate = 100.0f;  // バーストモード
    }

    // Lightning（雷撃）
    {
        auto& settings = effectSettings_[static_cast<int>(EffectType::Lightning)];
        settings.velocityMin = Vector3{-6.0f, 0.0f, -6.0f};     // 横方向の拡散
        settings.velocityMax = Vector3{6.0f, 1.0f, 6.0f};       // ほぼ横に広がる
        settings.accelMin = Vector3{0.0f, -1.0f, 0.0f};         // 軽い重力
        settings.accelMax = Vector3{0.0f, -1.0f, 0.0f};
        settings.startScaleMin = Vector3{0.001f, 0.001f, 0.001f};  // 完全に見えないサイズから
        settings.startScaleMax = Vector3{0.001f, 0.001f, 0.001f};  // 完全に見えないサイズから
        settings.endScaleMin = Vector3{0.2f, 2.0f, 0.2f};       // 細長い電撃に成長
        settings.endScaleMax = Vector3{0.4f, 4.0f, 0.4f};       // 縦長の電撃に成長
        settings.startColorMin = Vector4{1.0f, 1.0f, 1.0f, 0.0f};   // 完全に透明から
        settings.startColorMax = Vector4{1.0f, 1.0f, 1.0f, 0.0f};   // 完全に透明から
        settings.endColorMin = Vector4{0.6f, 0.8f, 1.0f, 0.8f};     // 青白い電撃色
        settings.endColorMax = Vector4{0.8f, 0.9f, 1.0f, 1.0f};     // 明るい電撃色
        settings.rotationVelocityMin = Vector3{-10.0f, -10.0f, -10.0f};
        settings.rotationVelocityMax = Vector3{10.0f, 10.0f, 10.0f};
        settings.lifeTimeMin = 0.15f; // 短めの寿命
        settings.lifeTimeMax = 0.4f;
        settings.particleCount = 12;  // パーティクル数を調整
        settings.emitRate = 100.0f;   // バーストモード
    }
}
