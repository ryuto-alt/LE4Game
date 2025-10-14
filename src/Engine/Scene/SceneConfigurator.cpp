#include "SceneConfigurator.h"
#include "UnoEngine.h"
#include "DirectXCommon.h"
#include "Camera.h"
#include "GameObject/Player.h"
#include "GameObject/Enemy.h"
#include "GameObject/FPSCamera.h"
#include "PostProcess.h"
#include "Manager/LightManager.h"
#include "Skybox.h"
#include "Object3d.h"
#include <cmath>

SceneData SceneConfigurator::LoadSceneFromJSON(const std::string& jsonPath) {
    return SceneLoader::LoadSceneData(jsonPath);
}

void SceneConfigurator::ApplySceneData(
    const SceneData& sceneData,
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    Camera* camera,
    std::unique_ptr<Player>& player,
    std::unique_ptr<Enemy>& enemy,
    std::vector<std::unique_ptr<Object3d>>& sceneObjects,
    std::unique_ptr<Skybox>& skybox,
    std::unique_ptr<LightManager>& lightManager,
    std::unique_ptr<FPSCamera>& fpsCamera,
    std::unique_ptr<PostProcess>& postProcess,
    bool& skyboxEnabled,
    float& fisheyeStrength,
    float& fisheyeRadius) {

    UnoEngine* engine = UnoEngine::GetInstance();

    // Core初期化
    postProcess = std::make_unique<PostProcess>();
    postProcess->Initialize(dxCommon, srvManager);
    dxCommon->SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    lightManager = std::make_unique<LightManager>();
    lightManager->Initialize();

    // 各セクションを適用
    ApplyEnvironment(sceneData, skyboxEnabled);
    ApplyCamera(sceneData, camera, fpsCamera);
    ApplyPostProcess(sceneData, postProcess, fisheyeStrength, fisheyeRadius);
    ApplyPlayer(sceneData, player, camera, engine);
    ApplyEnemy(sceneData, enemy, player.get(), camera);
    ApplyObjects(sceneData, sceneObjects, engine);
    ApplyAudio(sceneData, engine);
}

void SceneConfigurator::ApplyEnvironment(const SceneData& data, bool& skyboxEnabled) {
    skyboxEnabled = data.environment.skyboxEnabled;
    Object3d::SetEnvTex(data.environment.environmentMap);
}

void SceneConfigurator::ApplyCamera(const SceneData& data, Camera* camera, std::unique_ptr<FPSCamera>& fpsCamera) {
    // FOVを度数からラジアンに変換
    float fovRadians = data.camera.fovDegrees * 3.14159265358979323846f / 180.0f;
    camera->SetFov(fovRadians);

    fpsCamera = std::make_unique<FPSCamera>();
    fpsCamera->Initialize(true);
    fpsCamera->SetMouseLookEnabled(data.camera.mouseLookEnabled);
}

void SceneConfigurator::ApplyPostProcess(const SceneData& data, std::unique_ptr<PostProcess>& postProcess,
                                        float& fisheyeStrength, float& fisheyeRadius) {
    fisheyeStrength = data.postProcess.fisheyeStrength;
    fisheyeRadius = data.postProcess.fisheyeRadius;
    postProcess->SetFisheyeStrength(fisheyeStrength);
    postProcess->SetFisheyeRadius(fisheyeRadius);

    const auto& hp = data.postProcess.horrorParams;
    postProcess->SetHorrorParams(hp.vignette, hp.aberration, hp.noise, hp.scanlines, hp.distortion);
}

void SceneConfigurator::ApplyPlayer(const SceneData& data, std::unique_ptr<Player>& player,
                                   Camera* camera, UnoEngine* engine) {
    player = std::make_unique<Player>();
    player->Initialize(camera, data.player.useFPSCamera, data.player.enableCollision);
    player->SetupCamera(engine);
    player->SetPosition(data.player.position);
}

void SceneConfigurator::ApplyEnemy(const SceneData& data, std::unique_ptr<Enemy>& enemy,
                                  Player* player, Camera* camera) {
    if (!data.enemies.empty()) {
        enemy = std::make_unique<Enemy>();
        enemy->Initialize(camera);
        enemy->SetPosition(data.enemies[0].position);
        enemy->SetPlayer(player);
    }
}

void SceneConfigurator::ApplyObjects(const SceneData& data, std::vector<std::unique_ptr<Object3d>>& sceneObjects,
                                    UnoEngine* engine) {
    sceneObjects = SceneLoader::CreateObjects(data, engine);
}

void SceneConfigurator::ApplyAudio(const SceneData& data, UnoEngine* engine) {
    const auto& bgm = data.audio.bgm;
    if (!bgm.name.empty() && !bgm.path.empty()) {
        engine->LoadAudio(bgm.name, bgm.path);
        engine->PlayAudio(bgm.name, bgm.loop, bgm.volume);
    }
}
