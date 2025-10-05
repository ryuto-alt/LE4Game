#include "Particle3DDemo.h"
#ifdef _DEBUG
#include <imgui.h>
#endif

void Particle3DDemo::Initialize() {
    // 初期値の設定
    effectPosition_ = Vector3{0.0f, 0.0f, 0.0f};
    selectedEffectType_ = HitEffect3D::EffectType::Normal;
    autoTrigger_ = false;
    autoTriggerInterval_ = 2.0f;
    autoTriggerTimer_ = 0.0f;
    showDebugInfo_ = true;
}

void Particle3DDemo::Update() {
    // 何も処理しない（キーボード入力はGamePlaySceneで処理）
}

void Particle3DDemo::DrawImGui() {
#ifdef _DEBUG
    if (ImGui::Begin("3D Particle Effect Demo")) {

        // エフェクト発生位置の設定
        ImGui::Text("Effect Position:");
        ImGui::SliderFloat3("Position", &effectPosition_.x, -10.0f, 10.0f);

        ImGui::Separator();

        // エフェクトタイプの選択
        ImGui::Text("Effect Type:");
        const char* effectTypes[] = { "Normal", "Critical", "Impact", "Explosion", "Lightning" };
        int currentType = static_cast<int>(selectedEffectType_);
        if (ImGui::Combo("Type", &currentType, effectTypes, IM_ARRAYSIZE(effectTypes))) {
            selectedEffectType_ = static_cast<HitEffect3D::EffectType>(currentType);
        }

        ImGui::Separator();

        // エフェクト発生ボタン
        if (ImGui::Button("Trigger Selected Effect")) {
            TriggerEffectAtPosition();
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop All Effects")) {
            EffectManager3D::GetInstance()->StopAllEffects();
        }

        ImGui::Separator();

        // 個別エフェクトボタン
        if (ImGui::Button("Normal Hit (F1)")) {
            EffectManager3D::GetInstance()->PlayNormalHit(effectPosition_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Critical Hit (F2)")) {
            EffectManager3D::GetInstance()->PlayCriticalHit(effectPosition_);
        }

        if (ImGui::Button("Impact Hit (F3)")) {
            EffectManager3D::GetInstance()->PlayImpactHit(effectPosition_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Explosion (F4)")) {
            EffectManager3D::GetInstance()->PlayExplosion(effectPosition_);
        }

        if (ImGui::Button("Lightning Hit (F5)")) {
            EffectManager3D::GetInstance()->PlayLightningHit(effectPosition_);
        }

        ImGui::Separator();

        // 自動発生設定
        ImGui::Checkbox("Auto Trigger", &autoTrigger_);
        if (autoTrigger_) {
            ImGui::SliderFloat("Interval (sec)", &autoTriggerInterval_, 0.5f, 5.0f);
            ImGui::Text("Next trigger in: %.1f sec", autoTriggerInterval_ - autoTriggerTimer_);
        }

        ImGui::Separator();

        // デバッグ情報の表示切り替え
        ImGui::Checkbox("Show Debug Info", &showDebugInfo_);

        if (showDebugInfo_) {
            ImGui::Separator();
            ImGui::Text("Debug Information:");

            // エフェクトマネージャーの状態
            uint32_t activeCount = EffectManager3D::GetInstance()->GetActiveEffectCount();
            bool anyPlaying = EffectManager3D::GetInstance()->IsAnyEffectPlaying();

            ImGui::Text("Active Effects: %u", activeCount);
            ImGui::Text("Any Playing: %s", anyPlaying ? "Yes" : "No");

            // 詳細デバッグ情報
            if (ImGui::TreeNode("Detailed Debug")) {
                std::string debugInfo = EffectManager3D::GetInstance()->GetDebugInfo();
                ImGui::TextWrapped("%s", debugInfo.c_str());
                ImGui::TreePop();
            }
        }

        ImGui::Separator();

        // 操作説明
        ImGui::Text("Controls:");
        ImGui::Text("F1-F5: Trigger specific effects");
        ImGui::Text("Space: Random effects");
    }
    ImGui::End();
#endif
}

void Particle3DDemo::TriggerEffectAtPosition() {
    EffectManager3D::GetInstance()->TriggerHitEffect(effectPosition_, selectedEffectType_);
}

void Particle3DDemo::UpdateAutoTrigger() {
    if (!autoTrigger_) {
        autoTriggerTimer_ = 0.0f;
        return;
    }

    autoTriggerTimer_ += 1.0f / 60.0f; // 60FPS想定

    if (autoTriggerTimer_ >= autoTriggerInterval_) {
        // ランダムなエフェクトタイプを選択
        int randomType = rand() % 5;  // Lightningを含む
        HitEffect3D::EffectType type = static_cast<HitEffect3D::EffectType>(randomType);
        
        // ランダムな位置でエフェクト発生
        Vector3 randomPos = {
            effectPosition_.x + (rand() % 200 - 100) / 100.0f * 2.0f,  // ±2.0f の範囲
            effectPosition_.y + (rand() % 200 - 100) / 100.0f * 2.0f,
            effectPosition_.z + (rand() % 200 - 100) / 100.0f * 2.0f
        };
        
        EffectManager3D::GetInstance()->TriggerHitEffect(randomPos, type);
        
        autoTriggerTimer_ = 0.0f;
    }
}
