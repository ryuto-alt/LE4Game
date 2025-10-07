#include "UnoEngine.h"
#include "AABBCollision.h" // AABBコリジョンシステム
#include "InstancedRenderer.h"
#include <cassert>
#include <algorithm>
#include <cctype>
#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 静的メンバ変数の実体化
UnoEngine* UnoEngine::instance_ = nullptr;

UnoEngine* UnoEngine::GetInstance() {
    if (!instance_) {
        instance_ = new UnoEngine();
    }
    return instance_;
}

void UnoEngine::DestroyInstance() {
    if (instance_) {
        instance_->Finalize();
        delete instance_;
        instance_ = nullptr;
    }
}

void UnoEngine::Initialize() {
    try {
        // WinAppの初期化
        winApp_ = std::make_unique<WinApp>();
        winApp_->Initialize();

        // DirectXCommonの初期化
        dxCommon_ = std::make_unique<DirectXCommon>();
        dxCommon_->Initialize(winApp_.get());

        // SRVマネージャの初期化
        srvManager_ = std::make_unique<SrvManager>();
        srvManager_->Initialize(dxCommon_.get());

        // ここで明示的にPreDrawを呼び出し、ディスクリプタヒープを設定
        // srvManager_->PreDraw();  // 描画時に呼ぶので初期化では不要

        // テクスチャマネージャの初期化
        TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

        // デフォルトテクスチャの事前読み込み
        // TextureManager::GetInstance()->LoadDefaultTexture();

        // ImGuiの初期化
        InitializeImGui();

        // 入力初期化
        input_ = std::make_unique<Input>();
        input_->Initialize(winApp_.get());

        // オーディオマネージャの初期化
        AudioManager::GetInstance()->Initialize();

        // スプライト共通部分の初期化
        spriteCommon_ = std::make_unique<SpriteCommon>();
        spriteCommon_->Initialize(dxCommon_.get());

        // カメラの作成と初期化
        camera_ = std::make_unique<Camera>();
        camera_->SetTranslate({ 0.0f, 0.0f, -5.0f });
        // ウィンドウハンドルを設定（エンジンレベルで自動設定）
        camera_->SetWindowHandle(winApp_->GetHwnd());
        // Object3dCommonは存在しないためコメントアウト
        // Object3dCommon::SetDefaultCamera(camera_.get());

        // パーティクルマネージャの初期化
        ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

        // 基本的なパーティクルグループの作成（必要になったら作成）
        // ParticleManager::GetInstance()->CreateParticleGroup("smoke", "Resources/particle/smoke.png");

        // 3Dパーティクルマネージャの初期化
        Particle3DManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get(), spriteCommon_.get());

        // 3Dエフェクトマネージャの初期化
        EffectManager3D::GetInstance()->Initialize();

        // AABBコリジョンマネージャの初期化
        Collision::AABBCollisionManager::Create();

        // 3D空間オーディオリスナーの初期化
        audioListener_ = std::make_unique<SpatialAudioListener>();
        audioListener_->SetPosition(Vector3{0.0f, 0.0f, 0.0f});

        // シーンマネージャーの取得と初期化
        SceneManager* sceneManager = SceneManager::GetInstance();
        sceneManager->SetDirectXCommon(dxCommon_.get());
        sceneManager->SetInput(input_.get());
        sceneManager->SetSpriteCommon(spriteCommon_.get());
        sceneManager->SetSrvManager(srvManager_.get());
        sceneManager->SetCamera(camera_.get());
        sceneManager->SetWinApp(winApp_.get());

        // 初期化時のGPU同期を実行（削除）
        // dxCommon_->CommandKick();

    }
    catch (const std::exception&) {
        // エラーは無視
    }
}

void UnoEngine::Update() {
    try {
        // デルタタイムを更新
        UpdateDeltaTime();

        // Windowsのメッセージ処理
        if (winApp_->ProcessMessage()) {
            endRequest_ = true;
            return;
        }

        // 終了リクエストがある場合は更新処理をスキップ
        if (endRequest_) {
            return;
        }

        // 入力更新（常に実行）
        input_->Update();

        // F11キーでフルスクリーン切り替え
        if (input_->TriggerKey(DIK_F11)) {
            winApp_->ToggleFullscreen();
        }

        // SRVヒープを描画前に明示的に設定
        if (srvManager_) {
            srvManager_->PreDraw();
        }

        // カメラの更新（常に実行して滑らかな視点移動を実現）
        camera_->Update();

        // パーティクルマネージャーの更新
        ParticleManager::GetInstance()->Update(camera_.get());

        // 3Dパーティクルマネージャの更新
        Particle3DManager::GetInstance()->Update(camera_.get());

        // 3Dエフェクトマネージャの更新
        EffectManager3D::GetInstance()->Update();

        // AABBコリジョンマネージャの更新
        if (Collision::AABBCollisionManager::GetInstance()) {
            Collision::AABBCollisionManager::GetInstance()->Update();
        }

        // 3D空間オーディオの更新
        UpdateSpatialAudio();

        // シーンマネージャーの更新（常に実行）
        SceneManager::GetInstance()->Update();

        // SceneManagerからの終了リクエストをチェック
        if (SceneManager::GetInstance()->ShouldExit()) {
            endRequest_ = true;
            return;
        }
    }
    catch (const std::exception&) {
        // エラーは無視
    }
}

void UnoEngine::Draw() {
    try {
        // DirectXの描画準備
        dxCommon_->Begin();

        // SRVヒープを描画前に明示的に設定
        if (srvManager_) {
            srvManager_->PreDraw();
        }

        // シーンマネージャーの描画
        SceneManager::GetInstance()->Draw();

        // パーティクルの描画
        ParticleManager::GetInstance()->Draw();

        // 3Dパーティクルの描画
        Particle3DManager::GetInstance()->Draw(camera_.get());

        // ImGuiの準備と描画
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetCommandList());

        // 描画終了
        dxCommon_->End();
    }
    catch (const std::exception&) {
        // エラーは無視
    }
}

void UnoEngine::Finalize() {
    // 既に終了処理済みの場合は何もしない
    if (finalized_) {
        return;
    }
    finalized_ = true;
    
    try {
        // 3D空間オーディオの解放（最初に）
        spatialAudioSources_.clear();
        audioListener_.reset();

        // シーンマネージャーの終了処理（オブジェクトやスプライトを解放）
        SceneManager::GetInstance()->Finalize();

        // エフェクトマネージャの終了処理
        EffectManager3D::GetInstance()->Finalize();

        // AABBコリジョンマネージャの終了処理
        Collision::AABBCollisionManager::Destroy();

        // パーティクルマネージャーの終了処理（シーンの直後に強制解放）
        ParticleManager::Finalize();

        // 3Dパーティクルマネージャの終了処理（シーンの直後に強制解放）
        Particle3DManager::Finalize();

        // カメラの解放（シーンの後）
        camera_.reset();

        // ImGuiの解放（DirectX12リソースを使用しているため早めに）
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        // テクスチャマネージャの解放（シングルトンを強制破棄）
        TextureManager::GetInstance()->Finalize();

        // オーディオマネージャの解放（シングルトンを強制破棄）
        AudioManager::DestroyInstance();

        // 古い衝突判定マネージャの終了処理は削除

        // スプライト共通部分の解放
        spriteCommon_.reset();

        // SRVマネージャの解放（DirectX12リソースを解放）
        srvManager_.reset();

        // 入力の解放
        input_.reset();

        // DirectXCommonの解放（最後にDirectX12デバイスを解放）
        dxCommon_.reset();

        // ウィンドウアプリの解放
        winApp_.reset();

    }
    catch (const std::exception&) {
        // エラーは無視
    }
}

void UnoEngine::Run() {
    // ゲームループ
    while (!IsEndRequested()) {
        // ImGuiの新しいフレーム
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 更新
        Update();

        // 描画
        Draw();
    }
}



// === 統合API実装 ===

// === オーディオシステム ===
bool UnoEngine::LoadAudio(const std::string& name, const std::string& filePath) {
    auto* audioManager = AudioManager::GetInstance();
    
    // ファイル拡張子を確認して適切な読み込み関数を選択
    std::string extension = filePath.substr(filePath.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == "wav") {
        return audioManager->LoadWAV(name, filePath);
    } else if (extension == "mp3") {
        return audioManager->LoadMP3(name, filePath);
    }
    
    OutputDebugStringA(("警告: サポートされていないオーディオ形式です: " + filePath + "\n").c_str());
    return false;
}

void UnoEngine::PlayAudio(const std::string& name, bool loop, float volume) {
    AudioManager::GetInstance()->Play(name, loop);
    if (volume != 1.0f) {
        AudioManager::GetInstance()->SetVolume(name, volume);
    }
}

void UnoEngine::StopAudio(const std::string& name) {
    AudioManager::GetInstance()->Stop(name);
}

void UnoEngine::SetAudioVolume(const std::string& name, float volume) {
    AudioManager::GetInstance()->SetVolume(name, volume);
}

bool UnoEngine::IsAudioPlaying(const std::string& name) {
    return AudioManager::GetInstance()->IsPlaying(name);
}

// === パーティクルシステム ===
bool UnoEngine::CreateParticleEffect(const std::string& name, const std::string& texturePath) {
    ParticleManager::GetInstance()->CreateParticleGroup(name, texturePath);
    return true; // TODO: エラーハンドリングの改善
}

void UnoEngine::PlayParticle(const std::string& name, const Vector3& position, int count) {
    ParticleManager::GetInstance()->Emit(name, position, count);
}

void UnoEngine::PlayParticle(const std::string& name, const Vector3& position, int count,
                            const Vector3& velocity, float lifeTime) {
    // より詳細なパラメータでパーティクルを発生
    ParticleManager::GetInstance()->Emit(
        name, position, count,
        velocity, velocity, // 速度の最小・最大を同じに
        Vector3{0.0f, 0.0f, 0.0f}, Vector3{0.0f, 0.0f, 0.0f}, // 加速度なし
        0.5f, 1.0f, // サイズ
        0.0f, 0.3f, // 終了サイズ
        Vector4{1.0f, 1.0f, 1.0f, 1.0f}, Vector4{1.0f, 1.0f, 1.0f, 1.0f}, // 開始色
        Vector4{1.0f, 1.0f, 1.0f, 0.0f}, Vector4{1.0f, 1.0f, 1.0f, 0.0f}, // 終了色
        0.0f, 0.0f, // 回転
        0.0f, 0.0f, // 回転速度
        lifeTime, lifeTime // 寿命
    );
}

// === 3Dオブジェクト作成システム ===
std::unique_ptr<Object3d> UnoEngine::CreateObject3D() {
    auto object = std::make_unique<Object3d>();
    object->Initialize(dxCommon_.get(), spriteCommon_.get());
    object->SetCamera(camera_.get());
    return object;
}

std::unique_ptr<Model> UnoEngine::LoadModel(const std::string& modelPath) {
    auto model = std::make_unique<Model>();

    // Modelを初期化（DirectXCommonを渡す）
    model->Initialize(dxCommon_.get());

    // パスの解析
    size_t lastSlash = modelPath.find_last_of("/\\");
    std::string directoryPath = (lastSlash != std::string::npos) ?
        modelPath.substr(0, lastSlash + 1) : "";
    std::string filename = (lastSlash != std::string::npos) ?
        modelPath.substr(lastSlash + 1) : modelPath;

    model->LoadFromObj(directoryPath, filename);
    return model;
}

std::unique_ptr<InstancedRenderer> UnoEngine::CreateInstancedRenderer(size_t maxInstances) {
    auto renderer = std::make_unique<InstancedRenderer>();
    renderer->Initialize(dxCommon_.get(), spriteCommon_.get(), maxInstances);
    return renderer;
}

// === アニメーションシステム ===
std::unique_ptr<AnimatedModel> UnoEngine::CreateAnimatedModel() {
    auto animatedModel = std::make_unique<AnimatedModel>();
    animatedModel->Initialize(dxCommon_.get());
    return animatedModel;
}

Animation UnoEngine::LoadAnimation(const std::string& directoryPath, const std::string& filename) {
    return LoadAnimationFile(directoryPath, filename);
}

// === 2Dスプライト作成システム ===
std::unique_ptr<Sprite> UnoEngine::CreateSprite(const std::string& texturePath) {
    // テクスチャを読み込み
    LoadTexture(texturePath);
    
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon_.get(), texturePath);
    return sprite;
}


// === 衝突判定システム ===
bool UnoEngine::CheckCollision(const Vector3& pos1, float radius1, const Vector3& pos2, float radius2) {
    // 簡易的な球同士の衝突判定(距離ベース)
    float dx = pos2.x - pos1.x;
    float dy = pos2.y - pos1.y;
    float dz = pos2.z - pos1.z;
    float distanceSq = dx * dx + dy * dy + dz * dz;
    float radiusSum = radius1 + radius2;
    return distanceSq <= (radiusSum * radiusSum);
}

// === シーン管理 ===
void UnoEngine::ChangeScene(const std::string& sceneName) {
    SceneManager::GetInstance()->ChangeScene(sceneName);
}

// === デバッグ情報 ===
void UnoEngine::ShowDebugInfo() {
#ifdef _DEBUG
    ImGui::Begin("UnoEngine デバッグ情報");

    // FPS情報
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    // カメラ情報
    Vector3 cameraPos = camera_->GetTranslate();
    ImGui::Text("カメラ位置: (%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);

    // メモリ使用量（概算）
    // 全パーティクルグループのパーティクル数を合計
    uint32_t totalParticles = 0;
    auto* particleManager = ParticleManager::GetInstance();
    // 基本的なパーティクルグループの数をチェック
    totalParticles += particleManager->GetParticleCount("explosion");
    totalParticles += particleManager->GetParticleCount("smoke");
    ImGui::Text("アクティブパーティクル数: %d", totalParticles);

    // 入力状態
    if (ImGui::TreeNode("入力状態")) {
        ImGui::Text("ESC: %s", input_->PushKey(DIK_ESCAPE) ? "押下中" : "未押下");
        ImGui::Text("SPACE: %s", input_->PushKey(DIK_SPACE) ? "押下中" : "未押下");
        ImGui::Text("W: %s", input_->PushKey(DIK_W) ? "押下中" : "未押下");
        ImGui::Text("A: %s", input_->PushKey(DIK_A) ? "押下中" : "未押下");
        ImGui::Text("S: %s", input_->PushKey(DIK_S) ? "押下中" : "未押下");
        ImGui::Text("D: %s", input_->PushKey(DIK_D) ? "押下中" : "未押下");
        ImGui::TreePop();
    }

    ImGui::End();
#endif
}

// === 3D空間オーディオシステム実装 ===
std::unique_ptr<SpatialAudioSource> UnoEngine::CreateSpatialAudioSource(const std::string& audioName, const Vector3& position) {
    auto spatialSource = std::make_unique<SpatialAudioSource>();
    
    if (spatialSource->Initialize(audioName, position)) {
        return spatialSource;
    }
    
    return nullptr;
}

void UnoEngine::SetAudioListenerPosition(const Vector3& position) {
    if (audioListener_) {
        audioListener_->SetPosition(position);
    }
}

void UnoEngine::SetAudioListenerOrientation(const Vector3& forward, const Vector3& up) {
    if (audioListener_) {
        audioListener_->SetOrientation(forward, up);
    }
}

void UnoEngine::UpdateSpatialAudio() {
    if (!audioListener_) return;
    
    // 全ての3D空間オーディオソースを更新
    for (auto& spatialSource : spatialAudioSources_) {
        if (spatialSource) {
            spatialSource->Update(audioListener_->GetPosition(), audioListener_->GetForward());
        }
    }
}

void UnoEngine::InitializeImGui() {
    try {
        // ImGui初期化
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // ImGuiのIO設定を取得
        ImGuiIO& io = ImGui::GetIO();

        // 日本語フォント設定
        ImFontConfig fontConfig;
        fontConfig.MergeMode = true;  // 既存のフォントと統合

        // 日本語フォントのパス（例：MS Gothic）
        // Windows標準フォントを使用
        const char* fontPath = "C:\\Windows\\Fonts\\msgothic.ttc";

        // 日本語の文字範囲を指定
        static const ImWchar japaneseFontRanges[] = {
            0x0020, 0x00FF, // 基本ラテン文字
            0x3000, 0x30FF, // 日本語（平仮名、カタカナ）
            0x31F0, 0x31FF, // カタカナ拡張
            0xFF00, 0xFFEF, // 全角文字
            0x4E00, 0x9FAF, // CJK統合漢字
            0,
        };

        // デフォルトフォント読み込み
        io.Fonts->AddFontDefault();

        // 日本語フォント読み込み
        io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, &fontConfig, japaneseFontRanges);

        // ファイルが見つからない場合のフォールバック処理
        if (!io.Fonts->Fonts.Size || io.Fonts->Fonts.Size <= 1) {
            // デフォルトフォントのみの場合は警告を出力
            OutputDebugStringA("WARNING: 日本語フォントが読み込めませんでした。フォールバックフォントを使用します。\n");

            // フォールバックフォントとしてWindows標準のフォントを試行
            const char* fallbackFonts[] = {
                "C:\\Windows\\Fonts\\meiryo.ttc",
                "C:\\Windows\\Fonts\\msgothic.ttc",
                "C:\\Windows\\Fonts\\YuGothM.ttc"
            };

            for (const char* fallbackFont : fallbackFonts) {
                io.Fonts->AddFontFromFileTTF(fallbackFont, 16.0f, &fontConfig, japaneseFontRanges);
                if (io.Fonts->Fonts.Size > 1) {
                    OutputDebugStringA(("日本語フォールバックフォントを読み込みました: " + std::string(fallbackFont) + "\n").c_str());
                    break;
                }
            }
        }

        // フォントテクスチャをビルド
        io.Fonts->Build();

        ImGui_ImplWin32_Init(winApp_->GetHwnd());

        // SrvManagerのディスクリプタヒープを使用
        ImGui_ImplDX12_Init(
            dxCommon_->GetDevice(),
            2, // SwapChainのバッファ数
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            srvManager_->GetDescriptorHeap().Get(),
            srvManager_->GetCPUDescriptorHandle(0), // ImGui用に0番を使用
            srvManager_->GetGPUDescriptorHandle(0)
        );

    }
    catch (const std::exception&) {
        // エラーは無視
    }
}

// === スムージングシステム実装 ===

float UnoEngine::NormalizeAngle(float angle) {
    while (angle > (float)M_PI) angle -= 2.0f * (float)M_PI;
    while (angle < -(float)M_PI) angle += 2.0f * (float)M_PI;
    return angle;
}

float UnoEngine::AngleDifference(float from, float to) {
    float diff = to - from;
    return NormalizeAngle(diff);
}

float UnoEngine::LerpAngle(float from, float to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float diff = AngleDifference(from, to);
    return NormalizeAngle(from + diff * t);
}

float UnoEngine::Lerp(float from, float to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return from + (to - from) * t;
}

Vector3 UnoEngine::LerpVector3(const Vector3& from, const Vector3& to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Vector3{
        Lerp(from.x, to.x, t),
        Lerp(from.y, to.y, t),
        Lerp(from.z, to.z, t)
    };
}

float UnoEngine::SmoothRotation(float current, float target, float speed, float deltaTime) {
    float lerpFactor = std::min(1.0f, speed * deltaTime);
    return LerpAngle(current, target, lerpFactor);
}

void UnoEngine::UpdateDeltaTime() {
    // DirectXCommonから実際の経過時間を取得
    realDeltaTime_ = dxCommon_->GetDeltaTime();

    // デルタタイムは実際の経過時間を使用
    deltaTime_ = realDeltaTime_;

    // デルタタイムの下限を設定（0除算を防ぐ）
    if (deltaTime_ < 0.0001f) {
        deltaTime_ = 0.0001f;
    }
}

// === 簡易化API実装 ===
void UnoEngine::LoadTexture(const std::string& path) {
    TextureManager::GetInstance()->LoadTexture(path);
}

std::unique_ptr<Skybox> UnoEngine::CreateSkybox() {
    auto skybox = std::make_unique<Skybox>();
    skybox->Initialize(dxCommon_.get(), srvManager_.get(), TextureManager::GetInstance());
    return skybox;
}

void UnoEngine::LoadSkybox(Skybox* skybox, const std::string& path) {
    skybox->LoadCubemap(path);
}

void UnoEngine::UpdateCameraMouse() {
    float deltaX, deltaY;
    input_->GetMouseMovement(deltaX, deltaY);
    camera_->ProcessMouseInput(deltaX, deltaY);
}

void UnoEngine::UpdateCameraRightStick() {
    float stickX = input_->GetXboxRightStickX();
    float stickY = input_->GetXboxRightStickY();
    
    // デッドゾーンを適用（スティックが少し動いただけでは反応しない）
    const float deadZone = 0.1f;
    if (abs(stickX) < deadZone) stickX = 0.0f;
    if (abs(stickY) < deadZone) stickY = 0.0f;
    
    camera_->ProcessRightStickInput(stickX, stickY);
}