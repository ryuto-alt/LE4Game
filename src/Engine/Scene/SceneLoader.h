#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Mymath.h"

// 前方宣言
class Object3d;
class Player;
class Enemy;
class Camera;
class PostProcess;
class FPSCamera;
class UnoEngine;

// シーンデータ構造体
struct SceneData {
    // 環境設定
    struct Environment {
        bool skyboxEnabled = false;
        std::string environmentMap;
    } environment;

    // カメラ設定
    struct CameraConfig {
        float fovDegrees = 60.0f;
        std::string mode = "fps";
        bool mouseLookEnabled = true;
    } camera;

    // ポストプロセス設定
    struct PostProcessConfig {
        float fisheyeStrength = 0.0f;
        float fisheyeRadius = 1.0f;
        struct HorrorParams {
            float vignette = 0.0f;
            float aberration = 0.0f;
            float noise = 0.0f;
            float scanlines = 0.0f;
            float distortion = 0.0f;
        } horrorParams;
    } postProcess;

    // オーディオ設定
    struct AudioConfig {
        struct BGM {
            std::string name;
            std::string path;
            bool loop = false;
            float volume = 1.0f;
        } bgm;
    } audio;

    // プレイヤー設定
    struct PlayerConfig {
        Vector3 position = {0, 0, 0};
        bool useFPSCamera = true;
        bool enableCollision = false;
    } player;

    // 敵設定
    struct EnemyConfig {
        Vector3 position = {0, 0, 0};
    };
    std::vector<EnemyConfig> enemies;

    // オブジェクト設定
    struct ObjectConfig {
        std::string name;
        std::string modelPath;
        Vector3 position = {0, 0, 0};
        Vector3 rotation = {0, 0, 0};
        Vector3 scale = {1, 1, 1};
        bool enableLighting = true;
        bool enableEnv = false;
        bool enableAnimation = false;
        struct CollisionConfig {
            bool enabled = false;
            std::string type;
        } collision;
    };
    std::vector<ObjectConfig> objects;
};

// SceneLoaderクラス
class SceneLoader {
public:
    SceneLoader() = default;
    ~SceneLoader() = default;

    // JSONファイルからシーンデータを読み込む
    static SceneData LoadSceneData(const std::string& jsonPath);

    // シーンデータからオブジェクトを生成
    static std::vector<std::unique_ptr<Object3d>> CreateObjects(
        const SceneData& data, UnoEngine* engine);

private:
    // JSON解析ヘルパー（実装ファイルで使用）
    static Vector3 ParseVector3(const void* j);
    static SceneData::Environment ParseEnvironment(const void* j);
    static SceneData::CameraConfig ParseCamera(const void* j);
    static SceneData::PostProcessConfig ParsePostProcess(const void* j);
    static SceneData::AudioConfig ParseAudio(const void* j);
    static SceneData::PlayerConfig ParsePlayer(const void* j);
    static std::vector<SceneData::EnemyConfig> ParseEnemies(const void* j);
    static std::vector<SceneData::ObjectConfig> ParseObjects(const void* j);
};
