#pragma once
// 基本システム
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "Camera.h"
#include "StepTimer.h"

// グラフィックス関連
#include "SpriteCommon.h"
#include "Sprite.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "Object3d.h"
#include "Model.h"
#include "Skybox.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "Particle3DManager.h"
#include "Particle3DEmitter.h"
#include "HitEffect3D.h"
#include "EffectManager3D.h"

// アニメーション関連
#include "AnimatedModel.h"
#include "Animation.h"
#include "AnimationPlayer.h"
#include "AnimationBlender.h"
#include "AnimationUtility.h"

// 衝突判定関連
#include "CollisionPrimitive.h"
#include "AABBCollision.h"

// オーディオ関連
#include "AudioManager.h"
#include "SpatialAudioSource.h"
#include "SpatialAudioListener.h"

// NavMesh関連
#include "Manager/NavMeshManager.h"

// ポストプロセス関連
#include "PostProcess.h"

// シーン管理
#include "SceneManager.h"
#include "IScene.h"
#include "Scene/SceneLoader.h"
#include "Scene/SceneConfigurator.h"

// リソース管理
#include "Resource/ResourcePreloader.h"

// ライト管理
#include "Manager/LightManager.h"

// 数学・ユーティリティ関連
#include "Mymath.h"
#include "Logger.h"
#include "StringUtility.h"

// ImGui関連
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include <memory>
#include <string>
#include <vector>

// UnoEngineクラス - DirectX12ゲームエンジン統合クラス
class UnoEngine {
public:
    // ========================================
    // 🧩 基本機能
    // ========================================

    // シングルトンインスタンスを取得
    static UnoEngine* GetInstance();

    // シングルトンインスタンスを破棄
    static void DestroyInst();

    // コピー禁止
    UnoEngine(const UnoEngine&) = delete;
    UnoEngine& operator=(const UnoEngine&) = delete;

    // 初期化
    void Initialize();

    // 更新
    void Update();

    // 描画
    void Draw();

    // 終了処理
    void Finalize();

    // ゲームループ実行
    void Run();

    // 終了リクエスト
    bool IsEnding() const { return endRequest_; }
    void RequestEnd() { endRequest_ = true; }

    // ========================================
    // ⌨️ 入力システム
    // ========================================

    bool IsKeyDown(int key) const { return input_->PushKey(key); }
    bool IsKeyTrig(int key) const { return input_->TriggerKey(key); }
    void GetMouseMove(float& deltaX, float& deltaY) { input_->GetMouseMovement(deltaX, deltaY); }
    void SetCursor(bool visible) { input_->SetMouseCursor(visible); }
    void ResetMouse() { input_->ResetMouseCenter(); }
    
    // ========================================
    // 🎮 Xboxコントローラー入力システム
    // ========================================

    bool IsXboxConn(int playerIndex = 0) const { return input_->IsXboxControllerConnected(playerIndex); }
    bool IsXboxDown(int button, int playerIndex = 0) const { return input_->IsXboxButtonPressed(button, playerIndex); }
    bool IsXboxTrig(int button, int playerIndex = 0) const { return input_->IsXboxButtonTriggered(button, playerIndex); }
    float GetLStickX(int playerIndex = 0) const { return input_->GetXboxLeftStickX(playerIndex); }
    float GetLStickY(int playerIndex = 0) const { return input_->GetXboxLeftStickY(playerIndex); }
    float GetRStickX(int playerIndex = 0) const { return input_->GetXboxRightStickX(playerIndex); }
    float GetRStickY(int playerIndex = 0) const { return input_->GetXboxRightStickY(playerIndex); }
    float GetLTrigger(int playerIndex = 0) const { return input_->GetXboxLeftTrigger(playerIndex); }
    float GetRTrigger(int playerIndex = 0) const { return input_->GetXboxRightTrigger(playerIndex); }
    
    // ========================================
    // 🎥 カメラシステム
    // ========================================

    // カメラ位置・回転
    void SetCamPos(const Vector3& position) { camera_->SetTranslate(position); }
    void SetCamRot(const Vector3& rotation) { camera_->SetRotate(rotation); }
    Vector3 GetCamPos() const { return camera_->GetTranslate(); }
    Vector3 GetCamRot() const { return camera_->GetRotate(); }

    // カメラ入力処理
    void ProcCamMouse(float deltaX, float deltaY) { camera_->ProcessMouseInput(deltaX, deltaY); }
    void SetCamSens(float sensitivity) { camera_->SetMouseSensitivity(sensitivity); }

    // カメラモード
    void SetCamMode(int mode) { camera_->SetCameraMode(mode); }
    int GetCamMode() const { return camera_->GetCameraMode(); }
    void ToggleCam() { camera_->ToggleCameraMode(); }

    // カメラ移動（フリーカメラモード用）
    void MoveCamFwd(float distance) { camera_->MoveForward(distance); }
    void MoveCamRgt(float distance) { camera_->MoveRight(distance); }
    void MoveCamUp(float distance) { camera_->MoveUp(distance); }

    // カメラの方向ベクトル取得
    Vector3 GetCamFwd() const { return camera_->GetForwardVector(); }
    Vector3 GetCamRgt() const { return camera_->GetRightVector(); }
    Vector3 GetCamUp() const { return camera_->GetUpVector(); }

    // カメラの視野角設定
    void SetCamFov(float fov) { camera_->SetFovY(fov); }

    // カメラスティック感度設定
    void SetStickSens(float sensitivity) { camera_->SetStickSensitivity(sensitivity); }
    float GetStickSens() const { return camera_->GetStickSensitivity(); }

    // オービットカメラ設定
    void SetOrbitTgt(const Vector3& target) { camera_->SetOrbitTarget(target); }
    void SetOrbitDst(float distance) { camera_->SetOrbitDistance(distance); }
    void SetOrbitHgt(float height) { camera_->SetOrbitHeight(height); }
    
    // ========================================
    // 🔊 オーディオシステム
    // ========================================

    // 基本オーディオ
    bool LoadAudio(const std::string& name, const std::string& filePath);
    void PlayAudio(const std::string& name, bool loop = false, float volume = 1.0f);
    void StopAudio(const std::string& name);
    void SetAudVol(const std::string& name, float volume);
    bool IsAudPlay(const std::string& name);

    // 3D空間オーディオ
    std::unique_ptr<SpatialAudioSource> CreateSAud(const std::string& audioName, const Vector3& position);
    void SetListPos(const Vector3& position);
    void SetListOri(const Vector3& forward, const Vector3& up = Vector3{0.0f, 1.0f, 0.0f});
    void UpdateSAud();  // 毎フレーム呼び出して3Dオーディオを更新
    
    // ========================================
    // 💨 パーティクル・オブジェクト生成
    // ========================================

    // パーティクル
    bool CreatePart(const std::string& name, const std::string& texturePath);
    void PlayPart(const std::string& name, const Vector3& position, int count = 10);
    void PlayPart(const std::string& name, const Vector3& position, int count,
                  const Vector3& velocity, float lifeTime = 3.0f);

    // 3Dオブジェクト
    std::unique_ptr<Object3d> CreateObj3();
    std::unique_ptr<Model> LoadModel(const std::string& modelPath);
    std::unique_ptr<Object3d> CreateObjM(const std::string& modelPath);
    std::unique_ptr<class InstancedRenderer> CreateInst(size_t maxInstances = 10000);

    // アニメーション
    std::unique_ptr<AnimatedModel> CreateAnim();
    Animation LoadAnim(const std::string& directoryPath, const std::string& filename);

    // ゲームオブジェクト
    std::unique_ptr<class Enemy> CreateEnemy(const Vector3& position);
    std::unique_ptr<class Enemy> CreateEnemy(const Vector3& position, const struct EnemyAIConfig& aiConfig);

    // 2Dスプライト
    std::unique_ptr<Sprite> CreateSpr(const std::string& texturePath);

    // テクスチャ・スカイボックス
    void LoadTex(const std::string& path);
    std::unique_ptr<Skybox> CreateSky();
    void LoadSkybox(Skybox* skybox, const std::string& path);
    
    // ========================================
    // 📷 カメラ更新ヘルパー
    // ========================================

    void UpdCamMouse(); // マウス入力でカメラ更新（一括処理）
    void UpdCamStick(); // 右スティック入力でカメラ更新（一括処理）

    // ========================================
    // 🌍 環境マップ
    // ========================================

    template<typename T>
    void SetEnvMap(T* obj) {
        Skybox::SetEnvMap(obj);
    }

    template<typename T>
    void SetEnvMap(std::unique_ptr<T>& obj) {
        Skybox::SetEnvMap(obj);
    }

    // ========================================
    // ⚙️ ユーティリティ
    // ========================================

    // 衝突判定
    bool ChkCollide(const Vector3& pos1, float radius1, const Vector3& pos2, float radius2);

    // 角度計算・スムージング
    float NormAngle(float angle);
    float AngleDiff(float from, float to);
    float LerpAngle(float from, float to, float t);
    float Lerp(float from, float to, float t);
    Vector3 LerpVector3(const Vector3& from, const Vector3& to, float t);
    float SmoothRot(float current, float target, float speed, float deltaTime);

    // 時間管理
    float GetDelta() const { return static_cast<float>(timer_.GetElapsedSeconds()); }
    double GetDeltaTime() const { return timer_.GetElapsedSeconds(); }
    uint32_t GetFPS() const { return timer_.GetFramesPerSecond(); }
    uint32_t GetFrameCount() const { return timer_.GetFrameCount(); }
    double GetTotalTime() const { return timer_.GetTotalSeconds(); }

    // FPS制限設定（Unreal Engine風）
    void SetTargetFPS(uint32_t fps);
    void SetDeltaSmoothing(bool enabled) { timer_.SetDeltaTimeSmoothing(enabled); }

    // シーン管理
    void ChgScene(const std::string& sceneName);

    // デバッグ
    void ShowDebug();
    
    // ========================================
    // 🧱 マネージャー取得
    // ========================================

    WinApp* GetWinApp() const { return winApp_.get(); }
    DirectXCommon* GetDXCom() const { return dxCommon_.get(); }
    Input* GetInput() const { return input_.get(); }
    Camera* GetCamera() const { return camera_.get(); }
    SpriteCommon* GetSprCom() const { return spriteCommon_.get(); }
    SrvManager* GetSrvMgr() const { return srvManager_.get(); }
    SceneManager* GetScnMgr() const { return SceneManager::GetInstance(); }
    TextureManager* GetTexMgr() const { return TextureManager::GetInstance(); }
    ParticleManager* GetPartMgr() const { return ParticleManager::GetInstance(); }
    Particle3DManager* GetPart3Mgr() const { return Particle3DManager::GetInstance(); }
    EffectManager3D* GetEff3Mgr() const { return EffectManager3D::GetInstance(); }
    AudioManager* GetAudMgr() const { return AudioManager::GetInstance(); }
    Collision::AABBCollisionManager* GetCollMgr() const { return Collision::AABBCollisionManager::GetInstance(); }

    // ========================================
    // 🧭 NavMesh システム
    // ========================================

    // NavMeshManagerを取得
    NavMeshManager* GetNavMgr() const { return navMeshManager_.get(); }

    // NavMesh初期化・生成
    void InitNav(const std::string& navMeshPath);
    void GenNav(const std::vector<std::unique_ptr<Object3d>>& sceneObjects, const std::string& filepath);
    bool LoadNavMesh(const std::string& filepath);

    // NavMesh設定
    NavMeshBuildSettings& GetNavSet();

    // NavMesh視覚化
    void SetNavVis(bool enabled);
    bool IsNavVis() const;
    void CreateNavVis();
    void RequestNavVisUpdate();  // 可視化の更新をリクエスト
    void DrawNavVis();

    // NavMesh更新
    void UpdateNavMesh();

    // ========================================
    // 🎨 ポストプロセス
    // ========================================

    // ポストプロセス取得
    PostProcess* GetPostProcess() const { return postProcess_.get(); }

    // ポストプロセス描画制御
    void BeginPostProcess();
    void EndPostProcess();

    // ホラーエフェクトパラメータ設定
    void SetHorrorParams(float time, float noiseIntensity, float distortionAmount,
                         float bloodAmount, float vignetteIntensity);

    // 魚眼レンズエフェクト設定
    void SetFisheyeStrength(float strength);
    void SetFisheyeRadius(float radius);

    // ========================================
    // 💡 ライト管理
    // ========================================

    // ライトマネージャー取得
    LightManager* GetLightMgr() const { return lightManager_.get(); }

    // ライト更新
    void UpdateLights();

    // 懐中電灯更新
    void UpdateFlashlight(const Vector3& position, const Vector3& direction);

    // ========================================
    // 📦 リソースプリロード
    // ========================================

    // アニメーションモデルのプリロード
    void PreloadAnimModel(const std::string& key, const std::string& directoryPath, const std::string& filename);
    void PreloadAnimModelLightweight(const std::string& key, const std::string& directoryPath, const std::string& filename);

    // プリロード済みモデル取得
    std::unique_ptr<AnimatedModel> GetPreloadedModel(const std::string& key);
    bool HasPreloadedModel(const std::string& key);

    // プリロード進捗取得
    float GetPreloadProgress();

    // プリロード済みリソース全削除
    void ClearPreloadedResources();

    // ========================================
    // 🎬 シーン読み込み
    // ========================================

    // JSONからシーンデータを読み込む（SceneLoaderを使用）
    SceneData LoadSceneFromJSON(const std::string& jsonPath);

private:
    // シングルトンインスタンス
    static UnoEngine* instance_;

    // コンストラクタ（シングルトン）
    UnoEngine() = default;
    // デストラクタ（シングルトン）
    ~UnoEngine() = default;
    
    // 終了処理済みフラグ
    bool finalized_ = false;

    // 基本コンポーネント
    std::unique_ptr<WinApp> winApp_;
    std::unique_ptr<DirectXCommon> dxCommon_;
    std::unique_ptr<Input> input_;
    std::unique_ptr<Camera> camera_;

    // グラフィックス関連コンポーネント
    std::unique_ptr<SpriteCommon> spriteCommon_;
    std::unique_ptr<SrvManager> srvManager_;

    // 3D空間オーディオ関連
    std::unique_ptr<SpatialAudioListener> audioListener_;
    std::vector<std::unique_ptr<SpatialAudioSource>> spatialAudioSources_;

    // NavMesh関連
    std::unique_ptr<NavMeshManager> navMeshManager_;

    // ポストプロセス関連
    std::unique_ptr<PostProcess> postProcess_;

    // ライト管理関連
    std::unique_ptr<LightManager> lightManager_;

    // ImGuiの初期化
    void InitializeImGui();

    // 終了リクエストフラグ
    bool endRequest_ = false;

    // 時間管理（高精度タイマー）
    StepTimer timer_;
};