#pragma once
#include "IScene.h"
#include "GameObject/Player.h"
#include "GameObject/FPSCamera.h"
#include "Skybox.h"
#include "Manager/LightManager.h"
#include "InstancedRenderer.h"
#include "PostProcess.h"
#include <memory>

class GamePlayScene : public IScene {
public:
    GamePlayScene() = default;
    ~GamePlayScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    void HandleInput();

    std::unique_ptr<Player> player_;
    std::unique_ptr<Object3d> ground_;
    std::unique_ptr<AnimatedModel> groundModel_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<LightManager> lightManager_;

    std::unique_ptr<Object3d> objeObject_;
    std::unique_ptr<AnimatedModel> objeModel_;

    bool skyboxEnabled_ = false;

    std::unique_ptr<FPSCamera> fpsCamera_;
    std::unique_ptr<PostProcess> postProcess_;

    // 魚眼レンズ強度（ImGui用）
    float fisheyeStrength_ = 2.58f;
    float fisheyeRadius_ = 1.5f;  // 魚眼レンズの範囲
};