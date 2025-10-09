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
        object3d_->SetRotation(Vector3{0.0f, 0.0f, 0.0f});
        object3d_->SetEnableLighting(true);
        object3d_->SetCamera(camera_);

        // 環境マップの設定
        if (enableEnvironmentMap) {
            OutputDebugStringA("Ground: Environment map is enabled\n");
            Object3d::SetEnvTex("Resources/Models/skybox/tex.dds");
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