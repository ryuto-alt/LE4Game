#pragma once
#include "UnoEngine.h"

class LightManager {
public:
    LightManager();
    ~LightManager();

    void Initialize();
    void Update(float deltaTime);
    void DrawImGui();

    // スポットライトをプレイヤー視点に追従させる
    void UpdateFlashlight(const Vector3& playerPosition, const Vector3& cameraRotation);
    
    // ライトデータの取得
    const DirectionalLight& GetDirectionalLight() const { return directionalLight_; }
    const SpotLight& GetSpotLight() const { return spotLight_; }
    
    // デバッグ表示の切り替え
    void ToggleDebugDisplay() { showDebugWindow_ = !showDebugWindow_; }
    bool IsDebugDisplayShown() const { return showDebugWindow_; }

    // 光度調整
    void SetLightBoost(float boost);
    void ResetLightBoost();
    
    // ライトの有効/無効
    bool IsDirectionalLightEnabled() const { return enableDirectionalLight_; }
    bool IsSpotLightEnabled() const { return enableSpotLight_; }

    // 恐怖による点滅速度の設定
    void SetFearFlickerIntensity(float intensity) { fearFlickerIntensity_ = intensity; }

    // ジャンプスケア時のライト設定
    void SetJumpscareLight(const SpotLight& light) {
        spotLight_ = light;
        enableSpotLight_ = true;
    }

private:
    void NormalizeDirectionalLightDirection();
    void UpdateLightIntensity();
    void UpdateFlickerEffect(float deltaTime);  // ちらつき効果の更新

    // ライトデータ
    DirectionalLight directionalLight_;
    SpotLight spotLight_;

    // ライトの有効/無効フラグ
    bool enableDirectionalLight_ = true;
    bool enableSpotLight_ = true;

    // バックアップ用の強度値
    float dirLightIntensityBackup_ = 1.0f;
    float spotLightIntensityBackup_ = 3.5f;

    // デバッグ表示フラグ
    bool showDebugWindow_ = true;

    // 遅延追従用の変数
    Vector3 smoothedLightPosition_ = {0.0f, 5.0f, -2.0f};
    Vector3 smoothedLightDirection_ = {0.0f, -1.0f, 0.3f};
    float followSmoothness_ = 0.1f;  // 0.0〜1.0 (小さいほど遅延が大きい)

    // ちらつき効果用の変数
    float flickerTimer_ = 0.0f;
    float flickerBaseIntensity_ = 3.5f;  // 基本の明るさ
    float flickerAmount_ = 0.25f;  // ちらつきの強さ（0.0〜1.0）
    float flickerSpeed_ = 3.0f;  // ちらつきの速度（遅め）

    // 点滅効果用の変数
    float blinkTimer_ = 0.0f;
    float nextBlinkTime_ = 5.0f;  // 次の点滅までの時間
    bool isBlinking_ = false;
    float blinkDuration_ = 0.0f;
    float blinkProgress_ = 0.0f;

    // デバッグモード用
    bool isDebugBrightMode_ = false;
    DirectionalLight originalDirectionalLight_;  // 元の設定を保存

    // 恐怖による点滅強度 (0.0～1.0)
    float fearFlickerIntensity_ = 0.0f;
};