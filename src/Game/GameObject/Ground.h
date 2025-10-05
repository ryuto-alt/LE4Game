#pragma once
#include "UnoEngine.h"

class Ground {
public:
    Ground();
    ~Ground();

    void Initialize(Camera* camera, DirectXCommon* dxCommon, bool enableCollision = false, bool enableEnvironmentMap = false);
    void Update();
    void Draw();
    void Finalize();
    
    // ライトの設定
    void SetDirectionalLight(const DirectionalLight& light);
    void SetSpotLight(const SpotLight& light);
    
    // 環境マップテクスチャの設定
    void SetEnvTex(const std::string& texturePath);

    // 環境マップの有効/無効
    void EnableEnv(bool enable);
    bool IsEnvEnabled() const;
    
    
    // 位置の設定
    void SetPosition(const Vector3& position);
    Vector3 GetPosition() const;

    // コリジョン用
    Object3d* GetObject() const { return object3d_.get(); }
    AnimatedModel* GetModel() const { return model_.get(); }

private:
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<AnimatedModel> model_;
    
    Camera* camera_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    
    Vector3 position_ = Vector3{0.0f, -0.1f, 0.0f};
};