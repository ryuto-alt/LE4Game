#include "LightManager.h"
#ifdef _DEBUG
#include "imgui.h"
#endif
#include <cmath>
#include <cstdlib>
#include <ctime>

LightManager::LightManager() {
}

LightManager::~LightManager() {
}

void LightManager::Initialize() {
    // ディレクショナルライトの初期設定（真上から照らす）
    directionalLight_.color = { 0.0f, 0.0f, 0.0f, 1.0f };
    directionalLight_.direction = { 0.0f, -1.0f, 0.0f };  // 真下方向（真上から照らす）
    directionalLight_.intensity = 0.010f;

    // アンビエントライトの初期設定（暗い面を防ぐための環境光）
    directionalLight_.ambientColor = { 0.3f, 0.3f, 0.35f };  // より明るい環境光
    directionalLight_.ambientIntensity = 0.5f;  // マテリアルの色が見えるように明るく設定

    // スポットライトの初期設定
    spotLight_.color = { 140.0f / 255.0f, 140.0f / 255.0f, 135.0f / 255.0f, 1.0f };  // 適度な明るさ RGB(140, 140, 135)
    spotLight_.position = { 0.0f, 5.0f, -2.0f };
    spotLight_.intensity = 4.0f;  // 適度な明るさに調整
    spotLight_.direction = { 0.0f, -1.0f, 0.3f };
    spotLight_.innerCone = cosf(30.0f * 3.14159265f / 180.0f);  // 30度
    // 距離減衰をさらに緩やかに（より遠くまで光が届く）
    // attenuation = {定数減衰, 線形減衰, 二次減衰}
    // 減衰を弱くして約30m先まで光が届くように設定
    spotLight_.attenuation = { 1.0f, 0.09f, 0.23f };  // Quadratic 0.23
    spotLight_.outerCone = cosf(45.0f * 3.14159265f / 180.0f);  // 45度

    // 初期値をバックアップ
    dirLightIntensityBackup_ = directionalLight_.intensity;
    spotLightIntensityBackup_ = spotLight_.intensity;

    // ランダムシードを初期化
    srand(static_cast<unsigned int>(time(nullptr)));
}

void LightManager::Update(float deltaTime) {
    UpdateLightIntensity();
    UpdateFlickerEffect(deltaTime);

#ifdef _DEBUG
    // F2キーでデバッグ明るさモードの切り替え
    static bool prevF2State = false;
    bool currentF2State = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;

    if (currentF2State && !prevF2State) {
        // F2が押された瞬間
        isDebugBrightMode_ = !isDebugBrightMode_;

        if (isDebugBrightMode_) {
            // デバッグモードON: 元の設定を保存してライトを最大に
            originalDirectionalLight_ = directionalLight_;
            directionalLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };  // RGB(255, 255, 255)
            directionalLight_.intensity = 100.0f;  // 最大の明るさ
            directionalLight_.ambientColor = { 1.0f, 1.0f, 1.0f };
            directionalLight_.ambientIntensity = 5.0f;
            OutputDebugStringA("LightManager: Debug bright mode ON (F2 pressed)\n");
        } else {
            // デバッグモードOFF: 元の設定に戻す
            directionalLight_ = originalDirectionalLight_;
            OutputDebugStringA("LightManager: Debug bright mode OFF (F2 pressed)\n");
        }
    }

    prevF2State = currentF2State;
#endif
}

void LightManager::DrawImGui() {
#ifdef _DEBUG
    if (!showDebugWindow_) return;
    
    ImGui::Begin("Lighting Settings (F \u30ad\u30fc\u3067\u8868\u793a\u5207\u66ff)");
    
    // ディレクショナルライト設定
    ImGui::Separator();
    ImGui::Text("Directional Light");
    ImGui::Checkbox("Enable Directional Light", &enableDirectionalLight_);
    if (enableDirectionalLight_) {
        ImGui::ColorEdit3("Dir Light Color", &directionalLight_.color.x);
        ImGui::SliderFloat3("Dir Light Direction", &directionalLight_.direction.x, -1.0f, 1.0f);
        ImGui::SliderFloat("Dir Light Intensity", &directionalLight_.intensity, 0.0f, 3.0f);

        // アンビエントライト設定（動的に計算される値を表示）
        ImGui::Separator();
        ImGui::Text("Ambient Light (prevents black faces)");
        ImGui::ColorEdit3("Ambient Color", &directionalLight_.ambientColor.x);
        ImGui::Text("Ambient Intensity: %.4f (0.013 x Dir Intensity)", directionalLight_.ambientIntensity);

        NormalizeDirectionalLightDirection();
    }
    
    // スポットライト設定
    ImGui::Separator();
    ImGui::Text("Spot Light");
    ImGui::Checkbox("Enable Spot Light", &enableSpotLight_);
    if (enableSpotLight_) {
        ImGui::ColorEdit3("Spot Light Color", &spotLight_.color.x);
        ImGui::DragFloat3("Spot Light Position", &spotLight_.position.x, 0.1f);
        ImGui::SliderFloat3("Spot Light Direction", &spotLight_.direction.x, -1.0f, 1.0f);
        ImGui::SliderFloat("Spot Light Intensity", &spotLight_.intensity, 0.0f, 5.0f);
        
        // コーン角度（度数で表示）
        float innerAngle = acosf(spotLight_.innerCone) * 180.0f / 3.14159265f;
        float outerAngle = acosf(spotLight_.outerCone) * 180.0f / 3.14159265f;
        
        if (ImGui::SliderFloat("Inner Cone Angle", &innerAngle, 0.0f, 90.0f)) {
            spotLight_.innerCone = cosf(innerAngle * 3.14159265f / 180.0f);
        }
        if (ImGui::SliderFloat("Outer Cone Angle", &outerAngle, 0.0f, 90.0f)) {
            spotLight_.outerCone = cosf(outerAngle * 3.14159265f / 180.0f);
        }
        
        // 減衰パラメータ
        ImGui::Text("Attenuation");
        ImGui::SliderFloat("Constant", &spotLight_.attenuation.x, 0.0f, 2.0f);
        ImGui::SliderFloat("Linear", &spotLight_.attenuation.y, 0.0f, 0.5f);
        ImGui::SliderFloat("Quadratic", &spotLight_.attenuation.z, 0.0f, 10.0f);
    }
    
    // 現在のライト値の表示
    ImGui::Separator();
    ImGui::Text("\u73fe\u5728\u306e\u30e9\u30a4\u30c8\u5024:");
    ImGui::Text("Directional Light:");
    ImGui::Text("  \u5f37\u5ea6: %.2f", directionalLight_.intensity);
    ImGui::Text("  \u65b9\u5411: (%.2f, %.2f, %.2f)", 
        directionalLight_.direction.x, directionalLight_.direction.y, directionalLight_.direction.z);
    ImGui::Text("Spot Light:");
    ImGui::Text("  \u5f37\u5ea6: %.2f", spotLight_.intensity);
    ImGui::Text("  \u4f4d\u7f6e: (%.2f, %.2f, %.2f)", 
        spotLight_.position.x, spotLight_.position.y, spotLight_.position.z);
    
    ImGui::End();
#endif
}

void LightManager::NormalizeDirectionalLightDirection() {
    float dirLength = sqrtf(
        directionalLight_.direction.x * directionalLight_.direction.x +
        directionalLight_.direction.y * directionalLight_.direction.y +
        directionalLight_.direction.z * directionalLight_.direction.z
    );
    
    if (dirLength > 0.001f) {
        directionalLight_.direction.x /= dirLength;
        directionalLight_.direction.y /= dirLength;
        directionalLight_.direction.z /= dirLength;
    }
}

void LightManager::UpdateLightIntensity() {
    // ディレクショナルライトの強度管理
    if (!enableDirectionalLight_) {
        if (directionalLight_.intensity > 0.0f) {
            dirLightIntensityBackup_ = directionalLight_.intensity;
        }
        directionalLight_.intensity = 0.0f;
    } else if (directionalLight_.intensity == 0.0f) {
        directionalLight_.intensity = dirLightIntensityBackup_;
    }

    // アンビエントライトの強度をディレクショナルライトの強度に比例させる
    // 基本値0.013に、ディレクショナルライトの強度を掛ける
    directionalLight_.ambientIntensity = 0.013f * directionalLight_.intensity;

    // スポットライトの強度管理
    if (!enableSpotLight_) {
        if (spotLight_.intensity > 0.0f) {
            spotLightIntensityBackup_ = spotLight_.intensity;
        }
        spotLight_.intensity = 0.0f;
    } else if (spotLight_.intensity == 0.0f) {
        spotLight_.intensity = spotLightIntensityBackup_;
    }
}

void LightManager::UpdateFlickerEffect(float deltaTime) {
    // 点滅タイマーを進める
    blinkTimer_ += deltaTime;

    // 恐怖強度に応じて点滅頻度を調整（より自然に）
    // 点滅の管理
    if (!isBlinking_ && blinkTimer_ >= nextBlinkTime_) {
        // 点滅開始
        isBlinking_ = true;
        blinkProgress_ = 0.0f;
        // 恐怖時は点滅の持続時間も短く、よりランダムに
        float baseDuration = 0.15f + (static_cast<float>(rand()) / RAND_MAX) * 0.25f;  // 0.15〜0.4秒
        blinkDuration_ = baseDuration / (1.0f + fearFlickerIntensity_ * 1.5f);
        blinkTimer_ = 0.0f;

        // 次の点滅までの間隔（恐怖が強いほど短くなるが、よりランダムに）
        if (fearFlickerIntensity_ > 0.1f) {
            // 恐怖が強い時: 不規則な短い間隔
            float minInterval = 0.1f / fearFlickerIntensity_;  // 恐怖が強いほど最小間隔が短く
            float maxInterval = 0.5f / fearFlickerIntensity_;  // 恐怖が強いほど最大間隔も短く
            nextBlinkTime_ = minInterval + (static_cast<float>(rand()) / RAND_MAX) * (maxInterval - minInterval);
        } else {
            // 恐怖が弱い時: 通常の間隔
            nextBlinkTime_ = 3.0f + (static_cast<float>(rand()) / RAND_MAX) * 5.0f;
        }
    }

    float blinkMultiplier = 1.0f;
    if (isBlinking_) {
        blinkProgress_ += deltaTime;

        if (blinkProgress_ >= blinkDuration_) {
            // 点滅終了
            isBlinking_ = false;
            blinkMultiplier = 1.0f;
        } else {
            // 点滅中: ライトが消えかかる効果（完全には消えない）
            float blinkPhase = blinkProgress_ / blinkDuration_;

            // 消えかかる → 復帰 のパターン（最低でも40%の明るさを維持）
            if (blinkPhase < 0.3f) {
                // 急激に暗くなる
                blinkMultiplier = 1.0f - (blinkPhase / 0.3f) * 0.6f;  // 1.0 → 0.4
            } else if (blinkPhase < 0.5f) {
                // 消えかかった状態で揺らぐ
                blinkMultiplier = 0.4f + sinf((blinkPhase - 0.3f) / 0.2f * 3.14159f * 4.0f) * 0.1f;
            } else {
                // 徐々に復帰
                blinkMultiplier = 0.4f + ((blinkPhase - 0.5f) / 0.5f) * 0.6f;  // 0.4 → 1.0
            }

            blinkMultiplier = blinkMultiplier < 0.4f ? 0.4f : blinkMultiplier;  // 最低40%
        }
    }

    // 通常のちらつきタイマーを進める（恐怖強度に応じて速度を上げる）
    float effectiveFlickerSpeed = flickerSpeed_ * (1.0f + fearFlickerIntensity_ * 5.0f);  // 恐怖が強いと5倍速（より自然に）
    flickerTimer_ += deltaTime * effectiveFlickerSpeed;

    // パーリンノイズ風の複雑なちらつきパターン
    float flicker1 = sinf(flickerTimer_) * 0.5f + 0.5f;
    float flicker2 = sinf(flickerTimer_ * 1.7f) * 0.5f + 0.5f;
    float flicker3 = sinf(flickerTimer_ * 2.3f) * 0.5f + 0.5f;

    // 複数の波を組み合わせて不規則なちらつきを作る
    float flickerValue = (flicker1 * 0.5f + flicker2 * 0.3f + flicker3 * 0.2f);

    // ちらつきの範囲を調整（恐怖時はちらつき量を増やす）
    float effectiveFlickerAmount = flickerAmount_ * (1.0f + fearFlickerIntensity_ * 2.0f);  // 恐怖時はちらつき量2倍
    effectiveFlickerAmount = effectiveFlickerAmount > 1.0f ? 1.0f : effectiveFlickerAmount;  // 最大100%

    float minIntensity = flickerBaseIntensity_ * (1.0f - effectiveFlickerAmount);
    float maxIntensity = flickerBaseIntensity_ * (1.0f + effectiveFlickerAmount * 0.3f);

    float baseIntensity = minIntensity + (maxIntensity - minIntensity) * flickerValue;

    // スポットライトの強度を更新（点滅効果を適用）
    if (enableSpotLight_) {
        spotLight_.intensity = baseIntensity * blinkMultiplier;
    }
}

void LightManager::SetLightBoost(float boost) {
    // スポットライトの強度をブースト
    spotLight_.intensity = spotLightIntensityBackup_ * boost;

    // ディレクショナルライトの強度もブースト
    directionalLight_.intensity = dirLightIntensityBackup_ * boost;
}

void LightManager::ResetLightBoost() {
    spotLight_.intensity = spotLightIntensityBackup_;
    directionalLight_.intensity = dirLightIntensityBackup_;
}

void LightManager::UpdateFlashlight(const Vector3& playerPosition, const Vector3& cameraRotation) {
    // カメラの回転から方向ベクトルを計算
    float cosY = cosf(cameraRotation.y);
    float sinY = sinf(cameraRotation.y);
    float cosX = cosf(cameraRotation.x);
    float sinX = sinf(cameraRotation.x);

    // ライトの目標位置（プレイヤーの目の位置）
    Vector3 targetPosition = playerPosition;
    targetPosition.y += 1.6f;  // 目の高さ

    // ライトの目標方向（カメラの向き）
    Vector3 targetDirection;
    targetDirection.x = sinY * cosX;
    targetDirection.y = -sinX;
    targetDirection.z = cosY * cosX;

    // 遅延付き追従（線形補間）
    smoothedLightPosition_.x += (targetPosition.x - smoothedLightPosition_.x) * followSmoothness_;
    smoothedLightPosition_.y += (targetPosition.y - smoothedLightPosition_.y) * followSmoothness_;
    smoothedLightPosition_.z += (targetPosition.z - smoothedLightPosition_.z) * followSmoothness_;

    smoothedLightDirection_.x += (targetDirection.x - smoothedLightDirection_.x) * followSmoothness_;
    smoothedLightDirection_.y += (targetDirection.y - smoothedLightDirection_.y) * followSmoothness_;
    smoothedLightDirection_.z += (targetDirection.z - smoothedLightDirection_.z) * followSmoothness_;

    // スポットライトに適用
    spotLight_.position = smoothedLightPosition_;
    spotLight_.direction = smoothedLightDirection_;

    // 方向ベクトルを正規化
    float length = sqrtf(
        spotLight_.direction.x * spotLight_.direction.x +
        spotLight_.direction.y * spotLight_.direction.y +
        spotLight_.direction.z * spotLight_.direction.z
    );
    if (length > 0.001f) {
        spotLight_.direction.x /= length;
        spotLight_.direction.y /= length;
        spotLight_.direction.z /= length;
    }
}