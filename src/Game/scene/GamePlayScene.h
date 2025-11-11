#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "GameObject/Player.h"
#include "GameObject/Enemy.h"
#include "GameObject/Orb.h"
#include "GameObject/FPSCamera.h"
#include "Skybox.h"
#include "Manager/LightManager.h"
#include "InstancedRenderer.h"
#include "PostProcess.h"
#include "Scene/SceneConfigurator.h"
#include "../Utils/JsonLoader.h"
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
    std::unique_ptr<Sprite> fadeSprite_;  // 暗転用スプライト

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

    // カリング統計
    struct CullingStats {
        int totalObjects = 0;
        int visibleObjects = 0;
        int culledObjects = 0;
        int visibleMeshes = 0;
        int culledMeshes = 0;
        float cullingRate = 0.0f;
    } cullingStats_;

    // ゲームオーバー関連
    bool isGameOver_ = false;
    const float GAMEOVER_DISTANCE = 1.5f;  // エネミーとプレイヤーの距離5m

    // ジャンプスケア関連
    bool jumpscareStarted_ = false;  // ジャンプスケアが開始されたか

    // キャプチャカウンター（3回まで）
    int captureCount_ = 0;
    static constexpr int MAX_CAPTURES = 3;

    // 初期位置
    Vector3 playerInitialPos_ = {0.0f, 0.0f, 0.0f};
    Vector3 enemyInitialPos_ = {15.0f, 0.0f, 0.0f};

    // リスポーン処理用
    enum class RespawnState {
        None,
        FadeOut,
        Respawning,
        FadeIn
    };
    RespawnState respawnState_ = RespawnState::None;
    float respawnTimer_ = 0.0f;
    static constexpr float FADE_DURATION = 1.0f;  // 暗転の長さ
    float fadeAlpha_ = 0.0f;  // 0.0f = 透明, 1.0f = 完全に黒

    void UpdateRespawn(float deltaTime);
    void StartRespawn();
    void ResetPositions();
};