#pragma once
#include "IScene.h"
#include "GameObject/Player.h"
#include "GameObject/FPSCamera.h"
#include "Skybox.h"
#include "Manager/LightManager.h"
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
    void UpdateCamera();
    void DrawUI();

    std::unique_ptr<Player> player_;
    std::unique_ptr<Object3d> ground_;
    std::unique_ptr<AnimatedModel> groundModel_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<LightManager> lightManager_;

    std::unique_ptr<Object3d> objeObject_;
    std::unique_ptr<AnimatedModel> objeModel_;

    bool skyboxEnabled_ = false;

    std::unique_ptr<FPSCamera> fpsCamera_;

    // FPS表示用
    float displayedFps_ = 0.0f;
    float fpsUpdateTimer_ = 0.0f;

    // フラスタムカリング統計
    struct CullingStats {
        int totalObjects = 0;
        int visibleObjects = 0;
        int culledObjects = 0;
        int totalMeshes = 0;
        int visibleMeshes = 0;
        int culledMeshes = 0;
    };
    CullingStats cullingStats_;
};