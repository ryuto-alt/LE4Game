#include "SceneLoader.h"
#include "../../externals/tinygltf/json.hpp"
#include "UnoEngine.h"
#include "Object3d.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <cmath>

using json = nlohmann::json;

// 度数からラジアンへの変換
constexpr float DegreesToRadians(float degrees) {
    return degrees * 3.14159265358979323846f / 180.0f;
}

SceneData SceneLoader::LoadSceneData(const std::string& jsonPath) {
    SceneData data;

    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        Logger::Log("Failed to open scene file: " + jsonPath);
        return data;
    }

    try {
        json j;
        file >> j;

        // 各セクションをパース
        if (j.contains("environment")) {
            auto env = j["environment"];
            data.environment = ParseEnvironment(&env);
        }
        if (j.contains("camera")) {
            auto cam = j["camera"];
            data.camera = ParseCamera(&cam);
        }
        if (j.contains("postProcess")) {
            auto pp = j["postProcess"];
            data.postProcess = ParsePostProcess(&pp);
        }
        if (j.contains("audio")) {
            auto aud = j["audio"];
            data.audio = ParseAudio(&aud);
        }
        if (j.contains("player")) {
            auto player = j["player"];
            data.player = ParsePlayer(&player);
        }
        if (j.contains("enemies")) {
            auto enemies = j["enemies"];
            data.enemies = ParseEnemies(&enemies);
        }
        if (j.contains("objects")) {
            auto objects = j["objects"];
            data.objects = ParseObjects(&objects);
        }

        Logger::Log("Successfully loaded scene: " + jsonPath);
    }
    catch (const std::exception& e) {
        Logger::Log("JSON parse error: " + std::string(e.what()));
    }

    return data;
}

std::vector<std::unique_ptr<Object3d>> SceneLoader::CreateObjects(
    const SceneData& data, UnoEngine* engine) {

    std::vector<std::unique_ptr<Object3d>> objects;

    for (const auto& config : data.objects) {
        auto obj = engine->CreateObjM(config.modelPath);
        if (obj) {
            obj->SetPosition(config.position);
            obj->SetRotation(config.rotation);
            obj->SetScale(config.scale);
            obj->SetEnableLighting(config.enableLighting);
            obj->EnableEnv(config.enableEnv);
            obj->SetEnableAnimation(config.enableAnimation);

            if (config.collision.enabled) {
                obj->EnableCollision(true, config.collision.type);
            }

            objects.push_back(std::move(obj));
        }
    }

    return objects;
}

Vector3 SceneLoader::ParseVector3(const void* jsonPtr) {
    const json& j = *static_cast<const json*>(jsonPtr);
    if (j.is_array() && j.size() >= 3) {
        return Vector3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
    }
    return Vector3{0, 0, 0};
}

SceneData::Environment SceneLoader::ParseEnvironment(const void* jsonPtr) {
    const json& j = *static_cast<const json*>(jsonPtr);
    SceneData::Environment env;
    if (j.contains("skyboxEnabled")) env.skyboxEnabled = j["skyboxEnabled"].get<bool>();
    if (j.contains("environmentMap")) env.environmentMap = j["environmentMap"].get<std::string>();
    return env;
}

SceneData::CameraConfig SceneLoader::ParseCamera(const void* jsonPtr) {
    const json& j = *static_cast<const json*>(jsonPtr);
    SceneData::CameraConfig cam;
    if (j.contains("fovDegrees")) {
        cam.fovDegrees = j["fovDegrees"].get<float>();
    }
    if (j.contains("mode")) cam.mode = j["mode"].get<std::string>();
    if (j.contains("mouseLookEnabled")) cam.mouseLookEnabled = j["mouseLookEnabled"].get<bool>();
    return cam;
}

SceneData::PostProcessConfig SceneLoader::ParsePostProcess(const void* jsonPtr) {
    const json& j = *static_cast<const json*>(jsonPtr);
    SceneData::PostProcessConfig pp;
    if (j.contains("fisheyeStrength")) pp.fisheyeStrength = j["fisheyeStrength"].get<float>();
    if (j.contains("fisheyeRadius")) pp.fisheyeRadius = j["fisheyeRadius"].get<float>();

    if (j.contains("horrorParams")) {
        const auto& hp = j["horrorParams"];
        if (hp.contains("vignette")) pp.horrorParams.vignette = hp["vignette"].get<float>();
        if (hp.contains("aberration")) pp.horrorParams.aberration = hp["aberration"].get<float>();
        if (hp.contains("noise")) pp.horrorParams.noise = hp["noise"].get<float>();
        if (hp.contains("scanlines")) pp.horrorParams.scanlines = hp["scanlines"].get<float>();
        if (hp.contains("distortion")) pp.horrorParams.distortion = hp["distortion"].get<float>();
    }

    return pp;
}

SceneData::AudioConfig SceneLoader::ParseAudio(const void* jsonPtr) {
    const json& j = *static_cast<const json*>(jsonPtr);
    SceneData::AudioConfig audio;
    if (j.contains("bgm")) {
        const auto& bgm = j["bgm"];
        if (bgm.contains("name")) audio.bgm.name = bgm["name"].get<std::string>();
        if (bgm.contains("path")) audio.bgm.path = bgm["path"].get<std::string>();
        if (bgm.contains("loop")) audio.bgm.loop = bgm["loop"].get<bool>();
        if (bgm.contains("volume")) audio.bgm.volume = bgm["volume"].get<float>();
    }
    return audio;
}

SceneData::PlayerConfig SceneLoader::ParsePlayer(const void* jsonPtr) {
    const json& j = *static_cast<const json*>(jsonPtr);
    SceneData::PlayerConfig player;
    if (j.contains("position")) player.position = ParseVector3(&j["position"]);
    if (j.contains("useFPSCamera")) player.useFPSCamera = j["useFPSCamera"].get<bool>();
    if (j.contains("enableCollision")) player.enableCollision = j["enableCollision"].get<bool>();
    return player;
}

std::vector<SceneData::EnemyConfig> SceneLoader::ParseEnemies(const void* jsonPtr) {
    const json& j = *static_cast<const json*>(jsonPtr);
    std::vector<SceneData::EnemyConfig> enemies;

    if (j.is_array()) {
        for (const auto& e : j) {
            SceneData::EnemyConfig enemy;
            if (e.contains("position")) enemy.position = ParseVector3(&e["position"]);
            enemies.push_back(enemy);
        }
    }

    return enemies;
}

std::vector<SceneData::ObjectConfig> SceneLoader::ParseObjects(const void* jsonPtr) {
    const json& j = *static_cast<const json*>(jsonPtr);
    std::vector<SceneData::ObjectConfig> objects;

    if (j.is_array()) {
        for (const auto& o : j) {
            SceneData::ObjectConfig obj;

            if (o.contains("name")) obj.name = o["name"].get<std::string>();
            if (o.contains("modelPath")) obj.modelPath = o["modelPath"].get<std::string>();
            if (o.contains("position")) obj.position = ParseVector3(&o["position"]);
            if (o.contains("rotation")) obj.rotation = ParseVector3(&o["rotation"]);
            if (o.contains("scale")) obj.scale = ParseVector3(&o["scale"]);
            if (o.contains("enableLighting")) obj.enableLighting = o["enableLighting"].get<bool>();
            if (o.contains("enableEnv")) obj.enableEnv = o["enableEnv"].get<bool>();
            if (o.contains("enableAnimation")) obj.enableAnimation = o["enableAnimation"].get<bool>();

            if (o.contains("collision")) {
                const auto& col = o["collision"];
                if (col.contains("enabled")) obj.collision.enabled = col["enabled"].get<bool>();
                if (col.contains("type")) obj.collision.type = col["type"].get<std::string>();
            }

            objects.push_back(obj);
        }
    }

    return objects;
}
