#include "Ground.h"
#include "../Engine/Collision/AABBCollision.h"
#include <stdexcept>

Ground::Ground() {
}

Ground::~Ground() {
}

void Ground::Initialize(Camera* camera, DirectXCommon* dxCommon, bool enableCollision, bool enableEnvironmentMap) {
    camera_ = camera;
    dxCommon_ = dxCommon;

    UnoEngine* engine = UnoEngine::GetInstance();

    try {
        model_ = engine->CreateAnimatedModel();
        model_->Initialize(dxCommon_);

        // gltfファイルを読み込み
        OutputDebugStringA("Ground: Loading ground.gltf...\n");
        model_->LoadFromFile("Resources/Models/ground", "ground.gltf");

        // モデルデータの確認
        const ModelData& modelData = model_->GetModelData();
        char debugMsg[512];
        sprintf_s(debugMsg, "Ground: Loaded ground.gltf - Vertices: %zu\n", modelData.vertices.size());
        OutputDebugStringA(debugMsg);

        object3d_ = engine->CreateObject3D();
        object3d_->SetModel(model_.get());
        object3d_->SetPosition(position_);

        // Blenderで設定されたスケール情報を使用
        Vector3 blenderScale = modelData.rootTransform.scale;

        // スケールが0の場合はデフォルト値を使用
        if (blenderScale.x == 0.0f && blenderScale.y == 0.0f && blenderScale.z == 0.0f) {
            blenderScale = {40.0f, 40.0f, 40.0f};
            OutputDebugStringA("Ground: Using default scale (40, 40, 40)\n");
        }

        object3d_->SetScale(blenderScale);

        // デバッグ: 適用されたスケール値を出力
        sprintf_s(debugMsg, "Ground: Applied scale - X=%.3f, Y=%.3f, Z=%.3f\n",
                 blenderScale.x, blenderScale.y, blenderScale.z);
        OutputDebugStringA(debugMsg);

        sprintf_s(debugMsg, "Ground: Position - X=%.3f, Y=%.3f, Z=%.3f\n",
                 position_.x, position_.y, position_.z);
        OutputDebugStringA(debugMsg);

        object3d_->SetRotation(Vector3{0.0f, 0.0f, 0.0f});
        object3d_->SetEnableLighting(true);
        object3d_->SetCamera(camera_);

        // 環境マップの設定
        if (enableEnvironmentMap) {
            OutputDebugStringA("Ground: Environment map is enabled\n");
            Object3d::SetEnvTex("Resources/Models/skybox/rostock_laage_airport_4k.dds");
            object3d_->EnableEnv(true);
        } else {
            object3d_->EnableEnv(false);
            OutputDebugStringA("Ground: Environment map is disabled\n");
        }

        // コリジョン設定
        if (enableCollision && object3d_ && model_) {
            auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
            if (collisionManager) {
                Collision::AABB groundAABB = Collision::AABBExtractor::ExtractFromAnimatedModel(model_.get());
                collisionManager->RegisterObject(object3d_.get(), groundAABB, true, "Ground");
            }
        }
    } catch (const std::exception&) {
        // エラーは無視
    }
}

void Ground::Update() {
    if (!object3d_) return;
    object3d_->Update();
}

void Ground::Draw() {
    if (!object3d_) return;
    object3d_->Draw();
}

void Ground::Finalize() {
    if (object3d_) {
        object3d_.reset();
    }
    if (model_) {
        model_.reset();
    }
}

void Ground::SetDirectionalLight(const DirectionalLight& light) {
    if (object3d_) {
        object3d_->SetDirectionalLight(light);
    }
}

void Ground::SetSpotLight(const SpotLight& light) {
    if (object3d_) {
        object3d_->SetSpotLight(light);
    }
}

void Ground::SetEnvTex(const std::string& texturePath) {
    Object3d::SetEnvTex(texturePath);
}

void Ground::EnableEnv(bool enable) {
    if (object3d_) {
        object3d_->EnableEnv(enable);
    }
}

bool Ground::IsEnvEnabled() const {
    if (object3d_) {
        return object3d_->IsEnvEnabled();
    }
    return false;
}

void Ground::SetPosition(const Vector3& position) {
    position_ = position;
    if (object3d_) {
        object3d_->SetPosition(position_);
    }
}

Vector3 Ground::GetPosition() const {
    return position_;
}