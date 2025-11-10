#pragma once
#include <algorithm>

/// <summary>
/// EnemyのAIパラメータ設定
/// intelligence, mobility: 0.0 ~ 10.0 の範囲で設定可能
/// aggressiveness: 0.0 ~ 100.0 の範囲で設定可能（検知距離メートル）
/// </summary>
struct EnemyAIConfig {
    /// <summary>
    /// 知能（パス更新頻度・先読み能力）
    /// 0.0: パス更新2.0秒ごと、先読み無効なお
    /// 5.0: パス更新0.5秒ごと（デフォルト）
    /// 10.0: パス更新0.1秒ごと、先読み強化
    /// </summary>
    float intelligence = 8.0f;

    /// <summary>
    /// 攻撃性（検知範囲）
    /// 0.0: 検知範囲 0m（検知しない）
    /// 30.0: 検知範囲 30m
    /// 100.0: 検知範囲 100m
    /// 値をそのまま検知距離（メートル）として使用
    /// </summary>
    float aggressiveness = 20.0f;

    /// <summary>
    /// 機動力（移動速度）
    /// 0.0 ~ 10.0 → 移動速度 0.0 ~ 15.0 units/sec
    /// 5.0: 約7.5 units/sec（デフォルト）
    /// </summary>
    float mobility = 6.5f;

    /// <summary>
    /// 徘徊時の機動力（徘徊移動速度）
    /// 0.0 ~ 10.0 → 移動速度 0.0 ~ 15.0 units/sec
    /// 3.0: 約4.5 units/sec（デフォルト）
    /// </summary>
    float patrolMobility = 4.5f;

    /// <summary>
    /// 捜索時の機動力（音検知後の移動速度）
    /// 0.0 ~ 10.0 → 移動速度 0.0 ~ 15.0 units/sec
    /// 3.67: 約5.5 units/sec（デフォルト）
    /// </summary>
    float searchMobility = 5.5f;

    /// <summary>
    /// パラメータを有効範囲にクランプ
    /// </summary>
    void Clamp() {
        intelligence = std::clamp(intelligence, 0.0f, 10.0f);
        aggressiveness = std::clamp(aggressiveness, 0.0f, 100.0f);
        mobility = std::clamp(mobility, 0.0f, 10.0f);
        patrolMobility = std::clamp(patrolMobility, 0.0f, 10.0f);
        searchMobility = std::clamp(searchMobility, 0.0f, 10.0f);
    }
};
