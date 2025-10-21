#pragma once
#include "IScene.h"
#include "GameObject/Player.h"
#include "GameObject/Enemy.h"
#include "GameObject/FPSCamera.h"
#include "Skybox.h"
#include "Manager/LightManager.h"
#include "InstancedRenderer.h"
#include "PostProcess.h"
#include "Scene/SceneConfigurator.h"
#include "Sprite.h"
#include <memory>
#include <vector>

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
    std::unique_ptr<Enemy> enemy_;
    std::vector<std::unique_ptr<Object3d>> sceneObjects_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<LightManager> lightManager_;
    std::unique_ptr<FPSCamera> fpsCamera_;
    std::unique_ptr<PostProcess> postProcess_;
    std::unique_ptr<Sprite> fadeSprite_;

    SceneData sceneData_;
    bool skyboxEnabled_ = false;
    float fisheyeStrength_ = 2.58f;
    float fisheyeRadius_ = 1.5f;

    // 開始演出用
    bool isIntroPlaying_ = true;
    float introTimer_ = 0.0f;
    float introDuration_ = 6.0f; // 瞬き演出を含めて延長
    float fadeAlpha_ = 1.0f;
    float vignetteIntensity_ = 1.5f;
    bool introShakePlayed_ = false;

    // 瞬き演出用
    int blinkCount_ = 0;
    int maxBlinks_ = 3;
    float blinkTimer_ = 0.0f;
    float blinkOpenDuration_ = 0.4f;   // 目を開けている時間
    float blinkCloseDuration_ = 0.15f; // 目を閉じている時間
    bool isBlinkClosed_ = true;        // 瞬き中に目が閉じているか
};