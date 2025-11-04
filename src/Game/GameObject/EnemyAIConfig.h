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
    /// 0.0: パス更新2.0秒ごと、先読み無効
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
    float aggressiveness = 10.0f;

    /// <summary>
    /// 機動力（移動速度）
    /// 0.0: 移動速度 0.025f
    /// 5.0: 移動速度 0.125f（デフォルト）
    /// 10.0: 移動速度 0.25f
    /// </summary>
    float mobility = 7.05f;

    /// <summary>
    /// パラメータを有効範囲にクランプ
    /// </summary>
    void Clamp() {
        intelligence = std::clamp(intelligence, 0.0f, 10.0f);
        aggressiveness = std::clamp(aggressiveness, 0.0f, 100.0f);
        mobility = std::clamp(mobility, 0.0f, 10.0f);
    }
};
