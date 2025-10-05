#pragma once

#include "EffectManager3D.h"
#include "Input.h"
#include "Camera.h"
#include "Mymath.h"

// 3Dパーティクルエフェクトのデモクラス
class Particle3DDemo {
public:
    // コンストラクタ
    Particle3DDemo() = default;

    // デストラクタ
    ~Particle3DDemo() = default;

    // 初期化
    void Initialize();

    // 更新
    void Update();

    // ImGuiでのデバッグ表示
    void DrawImGui();

private:
    // エフェクトの発生位置
    Vector3 effectPosition_;

    // エフェクトの設定
    HitEffect3D::EffectType selectedEffectType_;

    // デモ用フラグ
    bool autoTrigger_;
    float autoTriggerInterval_;
    float autoTriggerTimer_;

    // デバッグ表示用
    bool showDebugInfo_;
    
    // エフェクト発生制御
    void TriggerEffectAtPosition();
    void UpdateAutoTrigger();
};
