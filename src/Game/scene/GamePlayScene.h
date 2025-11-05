#pragma once
#include "IScene.h"
#include "GameObject/Player.h"
#include "GameObject/Enemy.h"
#include "GameObject/Orb.h"
#include "GameObject/FPSCamera.h"
#include "Skybox.h"
#include "Manager/LightManager.h"
#include "InstancedRenderer.h"
#include "PostProcess.h"
#include "Scene/SceneConfigurator.h"
#include "NavMesh/NavMeshSystem.h"
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
    std::vector<std::unique_ptr<Orb>> orbs_;
    std::vector<std::unique_ptr<Object3d>> sceneObjects_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<LightManager> lightManager_;
    std::unique_ptr<FPSCamera> fpsCamera_;
    std::unique_ptr<PostProcess> postProcess_;
    std::unique_ptr<SpatialAudioListener> audioListener_;

    // New AI System
    std::unique_ptr<NavMeshSystem> navMeshSystem_;

    SceneData sceneData_;
    bool skyboxEnabled_ = false;
    float fisheyeStrength_ = 2.58f;
    float fisheyeRadius_ = 1.5f;

    // NavMeshログ
    std::vector<std::string> navMeshLogs_;
    void AddNavMeshLog(const std::string& message);
    void ClearNavMeshLogs();

    // NavMesh Debug表示フラグ
    bool showNavMeshDebug_ = false;

    // マウスカーソル表示フラグ (TABで切替)
    bool showMouseCursor_ = false;
};