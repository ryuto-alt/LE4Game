#pragma once

#include <unordered_map>
#include <string>
#include <list>
#include <random>
#include <memory>
#include "DirectXCommon.h"
#include "SRVManager.h"
#include "Mymath.h"
#include "Mymath.h"
#include "Camera.h"
#include "Model.h"
#include "Object3d.h"

// 前方宣言
class SpriteCommon;

// 3Dパーティクル1粒の情報
struct Particle3D {
    // 座標
    Vector3 position;
    // 速度
    Vector3 velocity;
    // 加速度
    Vector3 accel;
    // 回転
    Vector3 rotation;
    // 回転速度
    Vector3 rotationVelocity;
    // スケール
    Vector3 scale;
    // 初期スケール
    Vector3 startScale;
    // 最終スケール
    Vector3 endScale;
    // 色
    Vector4 color;
    // 初期色
    Vector4 startColor;
    // 最終色
    Vector4 endColor;
    // 経過時間
    float lifeTime;
    // 寿命
    float lifeTimeMax;
    // 生存フラグ
    bool isDead = false;
    // Object3D
    std::unique_ptr<Object3d> object3d;

    // デフォルトコンストラクタ
    Particle3D() = default;

    // コピーコンストラクタは削除
    Particle3D(const Particle3D&) = delete;
    Particle3D& operator=(const Particle3D&) = delete;

    // ムーブコンストラクタとムーブ代入演算子
    Particle3D(Particle3D&& other) noexcept
        : position(other.position)
        , velocity(other.velocity)
        , accel(other.accel)
        , rotation(other.rotation)
        , rotationVelocity(other.rotationVelocity)
        , scale(other.scale)
        , startScale(other.startScale)
        , endScale(other.endScale)
        , color(other.color)
        , startColor(other.startColor)
        , endColor(other.endColor)
        , lifeTime(other.lifeTime)
        , lifeTimeMax(other.lifeTimeMax)
        , isDead(other.isDead)
        , object3d(std::move(other.object3d)) {
    }

    Particle3D& operator=(Particle3D&& other) noexcept {
        if (this != &other) {
            position = other.position;
            velocity = other.velocity;
            accel = other.accel;
            rotation = other.rotation;
            rotationVelocity = other.rotationVelocity;
            scale = other.scale;
            startScale = other.startScale;
            endScale = other.endScale;
            color = other.color;
            startColor = other.startColor;
            endColor = other.endColor;
            lifeTime = other.lifeTime;
            lifeTimeMax = other.lifeTimeMax;
            isDead = other.isDead;
            object3d = std::move(other.object3d);
        }
        return *this;
    }
};

// 3Dパーティクルグループ
struct Particle3DGroup {
    // モデル
    std::shared_ptr<Model> model;
    // パーティクルのリスト
    std::list<Particle3D> particles;
};

// 3Dパーティクルマネージャクラス
class Particle3DManager {
private:
    // DirectXCommon
    DirectXCommon* dxCommon_ = nullptr;

    // SRVマネージャ
    SrvManager* srvManager_ = nullptr;

    // SpriteCommon
    SpriteCommon* spriteCommon_ = nullptr;

    // 乱数生成器
    std::mt19937 randomEngine_;

    // 3Dパーティクルグループコンテナ
    std::unordered_map<std::string, Particle3DGroup> particle3DGroups;

    // コピー禁止
    Particle3DManager(const Particle3DManager&) = delete;
    Particle3DManager& operator=(const Particle3DManager&) = delete;

    // シングルトンインスタンス
    static Particle3DManager* instance_;

    // コンストラクタ（シングルトン）
    Particle3DManager() = default;
    // デストラクタ
    ~Particle3DManager() = default;

public:
    // シングルトンインスタンスの取得
    static Particle3DManager* GetInstance();

    // 終了処理
    static void Finalize();

    // リソースの強制解放
    void ForceReleaseResources() {
        // 全3Dパーティクルグループのクリア
        particle3DGroups.clear();
    }

    // 初期化
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, SpriteCommon* spriteCommon);

    // 更新
    void Update(const Camera* camera);

    // 描画
    void Draw(const Camera* camera);

    // 3Dパーティクルグループの作成
    void CreateParticle3DGroup(const std::string& name, const std::string& modelFilePath);

    // 3Dパーティクルの発生
    void Emit3D(
        const std::string& name,
        const Vector3& position,
        uint32_t count,
        const Vector3& velocityMin = { -1.0f, -1.0f, -1.0f },
        const Vector3& velocityMax = { 1.0f, 1.0f, 1.0f },
        const Vector3& accelMin = { 0.0f, 0.0f, 0.0f },
        const Vector3& accelMax = { 0.0f, -9.8f, 0.0f },
        const Vector3& startScaleMin = { 0.5f, 0.5f, 0.5f },
        const Vector3& startScaleMax = { 1.0f, 1.0f, 1.0f },
        const Vector3& endScaleMin = { 0.0f, 0.0f, 0.0f },
        const Vector3& endScaleMax = { 0.0f, 0.0f, 0.0f },
        const Vector4& startColorMin = { 1.0f, 1.0f, 1.0f, 1.0f },
        const Vector4& startColorMax = { 1.0f, 1.0f, 1.0f, 1.0f },
        const Vector4& endColorMin = { 1.0f, 1.0f, 1.0f, 0.0f },
        const Vector4& endColorMax = { 1.0f, 1.0f, 1.0f, 0.0f },
        const Vector3& rotationMin = { 0.0f, 0.0f, 0.0f },
        const Vector3& rotationMax = { 0.0f, 0.0f, 0.0f },
        const Vector3& rotationVelocityMin = { 0.0f, 0.0f, 0.0f },
        const Vector3& rotationVelocityMax = { 0.0f, 0.0f, 0.0f },
        float lifeTimeMin = 1.0f,
        float lifeTimeMax = 3.0f);

    // デバッグ用：パーティクル数の取得
    uint32_t GetParticle3DCount(const std::string& name) {
        auto it = particle3DGroups.find(name);
        if (it != particle3DGroups.end()) {
            return static_cast<uint32_t>(it->second.particles.size());
        }
        return 0;
    }
};
