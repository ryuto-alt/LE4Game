#pragma once
#include "SceneLoader.h"
#include <memory>

// 前方宣言
class DirectXCommon;
class SrvManager;
class Camera;
class Player;
class Enemy;
class FPSCamera;
class PostProcess;
class LightManager;
class Skybox;
class Object3d;
class UnoEngine;

// SceneConfiguratorクラス
// JSONからロードしたシーンデータを実際のゲームオブジェクトに適用する
class SceneConfigurator {
public:
    SceneConfigurator() = default;
    ~SceneConfigurator() = default;

    // JSONファイルからシーンをロード
    SceneData LoadSceneFromJSON(const std::string& jsonPath);

    // シーンデータを適用してゲームオブジェクトを生成
    void ApplySceneData(
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
        float& fisheyeRadius
    );

private:
    void ApplyEnvironment(const SceneData& data, bool& skyboxEnabled);
    void ApplyCamera(const SceneData& data, Camera* camera, std::unique_ptr<FPSCamera>& fpsCamera);
    void ApplyPostProcess(const SceneData& data, std::unique_ptr<PostProcess>& postProcess,
                         float& fisheyeStrength, float& fisheyeRadius);
    void ApplyPlayer(const SceneData& data, std::unique_ptr<Player>& player, Camera* camera, UnoEngine* engine);
    void ApplyEnemy(const SceneData& data, std::unique_ptr<Enemy>& enemy, Player* player, Camera* camera);
    void ApplyObjects(const SceneData& data, std::vector<std::unique_ptr<Object3d>>& sceneObjects, UnoEngine* engine);
    void ApplyAudio(const SceneData& data, UnoEngine* engine);
};
