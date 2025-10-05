#pragma once

#include "Particle3DEmitter.h"
#include "Mymath.h"
#include <memory>
#include <vector>

// 3Dヒットエフェクトクラス
class HitEffect3D {
public:
    // エフェクトの種類
    enum class EffectType {
        Normal,     // 通常のヒット
        Critical,   // クリティカルヒット
        Impact,     // 衝撃
        Explosion,  // 爆発
        Lightning   // 雷撃
    };

    // コンストラクタ
    HitEffect3D();

    // デストラクタ
    ~HitEffect3D() = default;

    // 初期化
    void Initialize();

    // 更新
    void Update();

    // ヒットエフェクトの発生
    void TriggerHitEffect(const Vector3& position, EffectType type = EffectType::Normal);

    // エフェクトが再生中かどうか
    bool IsPlaying() const;

    // エフェクトの強制停止
    void Stop();

    // エフェクトタイプ別の設定
    void SetEffectSettings(EffectType type, const Vector3& velocityRange, 
                          const Vector4& startColor, const Vector4& endColor,
                          float lifeTime, uint32_t particleCount);

private:
    // エフェクト設定構造体
    struct EffectSettings {
        Vector3 velocityMin;
        Vector3 velocityMax;
        Vector3 accelMin;
        Vector3 accelMax;
        Vector3 startScaleMin;
        Vector3 startScaleMax;
        Vector3 endScaleMin;
        Vector3 endScaleMax;
        Vector4 startColorMin;
        Vector4 startColorMax;
        Vector4 endColorMin;
        Vector4 endColorMax;
        Vector3 rotationVelocityMin;
        Vector3 rotationVelocityMax;
        float lifeTimeMin;
        float lifeTimeMax;
        uint32_t particleCount;
        float emitRate;
    };

    // エフェクト設定の初期化
    void InitializeEffectSettings();

    // エミッター
    std::vector<std::unique_ptr<Particle3DEmitter>> emitters_;

    // エフェクト設定
    std::vector<EffectSettings> effectSettings_;

    // 現在のエフェクトタイプ
    EffectType currentEffectType_;

    // エフェクト再生中フラグ
    bool isPlaying_;
};
