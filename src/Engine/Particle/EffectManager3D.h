#pragma once

#include "HitEffect3D.h"
#include "Mymath.h"
#include <memory>
#include <vector>
#include <string>

// 3Dエフェクトマネージャークラス
class EffectManager3D {
private:
    // コピー禁止
    EffectManager3D(const EffectManager3D&) = delete;
    EffectManager3D& operator=(const EffectManager3D&) = delete;

    // コンストラクタ（シングルトン）
    EffectManager3D() = default;
    // デストラクタ
    ~EffectManager3D() = default;

public:
    // シングルトンインスタンスの取得
    static EffectManager3D* GetInstance() {
        static EffectManager3D instance;
        return &instance;
    }

    // 初期化
    void Initialize();
    
    // 終了処理
    void Finalize();

    // 更新
    void Update();

    // ヒットエフェクトの発生
    void TriggerHitEffect(const Vector3& position, 
                         HitEffect3D::EffectType type = HitEffect3D::EffectType::Normal);

    // 特定座標にエフェクト発生（簡単なインターフェース）
    void PlayNormalHit(const Vector3& position);
    void PlayCriticalHit(const Vector3& position);
    void PlayImpactHit(const Vector3& position);
    void PlayExplosion(const Vector3& position);
    void PlayLightningHit(const Vector3& position);

    // エフェクトの強制停止
    void StopAllEffects();

    // エフェクトが再生中かどうか
    bool IsAnyEffectPlaying() const;

    // アクティブなエフェクト数の取得
    uint32_t GetActiveEffectCount() const;

    // デバッグ情報の取得
    std::string GetDebugInfo() const;

    // 特別な雷撃エフェクトは削除し、通常のPlayLightningHitのみ使用

private:
    // ヒットエフェクトのプール
    std::vector<std::unique_ptr<HitEffect3D>> hitEffectPool_;

    // プールサイズ
    static const uint32_t POOL_SIZE = 10;

    // 次に使用するエフェクトのインデックス
    uint32_t nextEffectIndex_ = 0;

    // 使用可能なエフェクトを取得
    HitEffect3D* GetAvailableEffect();
};
