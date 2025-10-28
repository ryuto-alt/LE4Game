#pragma once
#include <algorithm>

/// <summary>
/// EnemyのAIパラメータ設定
/// 各パラメータは 0.0 ~ 10.0 の範囲で設定可能
/// デフォルト値は 5.0 で、現在のEnemyの標準的な強度
/// </summary>
struct EnemyAIConfig {
    /// <summary>
    /// 知能（パス更新頻度・先読み能力）
    /// 0.0: パス更新2.0秒ごと、先読み無効
    /// 5.0: パス更新0.5秒ごと（デフォルト）
    /// 10.0: パス更新0.1秒ごと、先読み強化
    /// </summary>
    float intelligence = 8.0f;

    /// <summary>
    /// 攻撃性（検知範囲）
    /// 0.0: 検知範囲 20.0f
    /// 5.0: 検知範囲 100.0f（デフォルト）
    /// 10.0: 検知範囲 200.0f
    /// </summary>
    float aggressiveness = 5.0f;

    /// <summary>
    /// 機動力（移動速度）
    /// 0.0: 移動速度 0.025f
    /// 5.0: 移動速度 0.125f（デフォルト）
    /// 10.0: 移動速度 0.25f
    /// </summary>
    float mobility = 5.0f;

    /// <summary>
    /// パラメータを0.0~10.0の範囲にクランプ
    /// </summary>
    void Clamp() {
        intelligence = std::clamp(intelligence, 0.0f, 10.0f);
        aggressiveness = std::clamp(aggressiveness, 0.0f, 10.0f);
        mobility = std::clamp(mobility, 0.0f, 10.0f);
    }
};
