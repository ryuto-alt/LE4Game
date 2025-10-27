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
#include "NavMesh/NavMesh.h"
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
    void GenerateAndSaveNavMesh(const std::string& filepath);

    std::unique_ptr<Player> player_;
    std::unique_ptr<Enemy> enemy_;
    std::vector<std::unique_ptr<Object3d>> sceneObjects_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<LightManager> lightManager_;
    std::unique_ptr<FPSCamera> fpsCamera_;
    std::unique_ptr<PostProcess> postProcess_;
    std::unique_ptr<NavMesh> navMesh_;

    SceneData sceneData_;
    bool skyboxEnabled_ = false;
    float fisheyeStrength_ = 2.58f;
    float fisheyeRadius_ = 1.5f;

    // NavMesh設定
    NavMeshBuildSettings navMeshSettings_;
    bool showNavMeshDebug_ = false;

    // NavMeshログ
    std::vector<std::string> navMeshLogs_;
    void AddNavMeshLog(const std::string& message);
    void ClearNavMeshLogs();
};