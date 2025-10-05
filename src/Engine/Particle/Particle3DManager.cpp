#include "Particle3DManager.h"
#include "Model.h"
#include "SpriteCommon.h"
#include <cassert>
#include <algorithm>

// 静的メンバ変数の初期化
Particle3DManager* Particle3DManager::instance_ = nullptr;

Particle3DManager* Particle3DManager::GetInstance() {
    if (!instance_) {
        instance_ = new Particle3DManager();
    }
    return instance_;
}

void Particle3DManager::Finalize() {
    if (instance_) {
        instance_->ForceReleaseResources();
        delete instance_;
        instance_ = nullptr;
    }
}

void Particle3DManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, SpriteCommon* spriteCommon) {
    // nullptrチェック
    assert(dxCommon);
    assert(srvManager);
    // spriteCommonはnullptrでも許可（必要ない場合があるため）

    // メンバ変数に保存
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;

    // 乱数エンジンの初期化
    std::random_device seed_gen;
    randomEngine_.seed(seed_gen());

    // デバッグ出力
    OutputDebugStringA("Particle3DManager: Initialized successfully\n");
}

void Particle3DManager::CreateParticle3DGroup(const std::string& name, const std::string& modelFilePath) {
    // 既に同名のグループが存在する場合は処理をスキップ
    if (particle3DGroups.find(name) != particle3DGroups.end()) {
        OutputDebugStringA(("Particle3DManager: Group already exists - " + name + "\n").c_str());
        return;
    }

    // 新規3Dパーティクルグループを作成
    Particle3DGroup group;

    // モデルの作成と読み込み
    group.model = std::make_shared<Model>();
    group.model->Initialize(dxCommon_);
    
    // モデルファイルの読み込み（Resources/particle/から読み込み）
    try {
        group.model->LoadFromObj("Resources/particle", modelFilePath);
        OutputDebugStringA(("Particle3DManager: Model loaded successfully - " + modelFilePath + "\n").c_str());
    }
    catch (const std::exception& e) {
        OutputDebugStringA(("Particle3DManager: Failed to load model - " + modelFilePath + ": " + e.what() + "\n").c_str());
        return;
    }

    // 3Dパーティクルグループを登録
    particle3DGroups[name] = std::move(group);

    // 登録成功をデバッグ出力
    OutputDebugStringA(("Particle3DManager: Created 3D particle group - " + name + "\n").c_str());
}

void Particle3DManager::Update(const Camera* camera) {
    // 全3Dパーティクルグループの更新
    for (auto& [name, group] : particle3DGroups) {
        // 各パーティクルの更新
        for (auto it = group.particles.begin(); it != group.particles.end(); ) {
            // 寿命チェック
            it->lifeTime += 1.0f / 60.0f; // 60FPS想定
            if (it->lifeTime >= it->lifeTimeMax) {
                // 寿命が尽きたら削除
                it = group.particles.erase(it);
                continue;
            }

            // 最初のフレームの場合はisDeadをfalseにして表示開始
            if (it->isDead) {
                it->isDead = false;
            }
            
            // パーティクルの更新
            // 速度に加速度を加算
            it->velocity.x += it->accel.x / 60.0f;
            it->velocity.y += it->accel.y / 60.0f;
            it->velocity.z += it->accel.z / 60.0f;

            // 位置に速度を加算
            it->position.x += it->velocity.x / 60.0f;
            it->position.y += it->velocity.y / 60.0f;
            it->position.z += it->velocity.z / 60.0f;

            // 回転を更新
            it->rotation.x += it->rotationVelocity.x / 60.0f;
            it->rotation.y += it->rotationVelocity.y / 60.0f;
            it->rotation.z += it->rotationVelocity.z / 60.0f;

            // 線形補間でスケールと色を更新
            float t = it->lifeTime / it->lifeTimeMax;
            
            // スケールの補間
            it->scale.x = (1.0f - t) * it->startScale.x + t * it->endScale.x;
            it->scale.y = (1.0f - t) * it->startScale.y + t * it->endScale.y;
            it->scale.z = (1.0f - t) * it->startScale.z + t * it->endScale.z;

            // 色の補間
            it->color.x = (1.0f - t) * it->startColor.x + t * it->endColor.x;
            it->color.y = (1.0f - t) * it->startColor.y + t * it->endColor.y;
            it->color.z = (1.0f - t) * it->startColor.z + t * it->endColor.z;
            it->color.w = (1.0f - t) * it->startColor.w + t * it->endColor.w;

            // Object3Dの更新（カメラのビュープロジェクション行列を使用）
            if (it->object3d && camera) {
                it->object3d->SetPosition(it->position);
                it->object3d->SetRotation(it->rotation);
                it->object3d->SetScale(it->scale);
                it->object3d->SetColor(it->color);
                it->object3d->Update(camera->GetViewMatrix(), camera->GetProjectionMatrix());
            }

            // 次のパーティクルへ
            ++it;
        }
    }
}

void Particle3DManager::Draw(const Camera* camera) {
    // パーティクルがない場合は描画しない
    bool hasParticles = false;
    for (auto& [name, group] : particle3DGroups) {
        if (!group.particles.empty()) {
            hasParticles = true;
            break;
        }
    }
    if (!hasParticles) {
        return;
    }

    // 各3Dパーティクルグループの描画
    for (auto& [name, group] : particle3DGroups) {
        // パーティクルがない場合はスキップ
        if (group.particles.empty()) {
            continue;
        }

        // 各パーティクルの描画
        for (auto& particle : group.particles) {
            // isDead（最初のフレーム）の場合は描画をスキップ
            if (!particle.isDead && particle.object3d) {
                particle.object3d->Draw();
            }
        }
    }
}

void Particle3DManager::Emit3D(
    const std::string& name,
    const Vector3& position,
    uint32_t count,
    const Vector3& velocityMin,
    const Vector3& velocityMax,
    const Vector3& accelMin,
    const Vector3& accelMax,
    const Vector3& startScaleMin,
    const Vector3& startScaleMax,
    const Vector3& endScaleMin,
    const Vector3& endScaleMax,
    const Vector4& startColorMin,
    const Vector4& startColorMax,
    const Vector4& endColorMin,
    const Vector4& endColorMax,
    const Vector3& rotationMin,
    const Vector3& rotationMax,
    const Vector3& rotationVelocityMin,
    const Vector3& rotationVelocityMax,
    float lifeTimeMin,
    float lifeTimeMax) {

    // 指定された名前の3Dパーティクルグループが存在するか確認
    auto it = particle3DGroups.find(name);
    assert(it != particle3DGroups.end());

    // パラメータのバリデーション（最小値 <= 最大値であることを確認）
    auto clampMinMax = [](float& min, float& max) {
        if (min > max) {
            std::swap(min, max);
        }
    };
    
    // ローカル変数でコピーしてバリデーション
    Vector3 validVelocityMin = velocityMin;
    Vector3 validVelocityMax = velocityMax;
    Vector3 validAccelMin = accelMin;
    Vector3 validAccelMax = accelMax;
    Vector3 validStartScaleMin = startScaleMin;
    Vector3 validStartScaleMax = startScaleMax;
    Vector3 validEndScaleMin = endScaleMin;
    Vector3 validEndScaleMax = endScaleMax;
    Vector4 validStartColorMin = startColorMin;
    Vector4 validStartColorMax = startColorMax;
    Vector4 validEndColorMin = endColorMin;
    Vector4 validEndColorMax = endColorMax;
    Vector3 validRotationMin = rotationMin;
    Vector3 validRotationMax = rotationMax;
    Vector3 validRotationVelocityMin = rotationVelocityMin;
    Vector3 validRotationVelocityMax = rotationVelocityMax;
    float validLifeTimeMin = lifeTimeMin;
    float validLifeTimeMax = lifeTimeMax;
    
    // バリデーションと修正
    clampMinMax(validVelocityMin.x, validVelocityMax.x);
    clampMinMax(validVelocityMin.y, validVelocityMax.y);
    clampMinMax(validVelocityMin.z, validVelocityMax.z);
    
    clampMinMax(validAccelMin.x, validAccelMax.x);
    clampMinMax(validAccelMin.y, validAccelMax.y);
    clampMinMax(validAccelMin.z, validAccelMax.z);
    
    clampMinMax(validStartScaleMin.x, validStartScaleMax.x);
    clampMinMax(validStartScaleMin.y, validStartScaleMax.y);
    clampMinMax(validStartScaleMin.z, validStartScaleMax.z);
    
    clampMinMax(validEndScaleMin.x, validEndScaleMax.x);
    clampMinMax(validEndScaleMin.y, validEndScaleMax.y);
    clampMinMax(validEndScaleMin.z, validEndScaleMax.z);
    
    clampMinMax(validStartColorMin.x, validStartColorMax.x);
    clampMinMax(validStartColorMin.y, validStartColorMax.y);
    clampMinMax(validStartColorMin.z, validStartColorMax.z);
    clampMinMax(validStartColorMin.w, validStartColorMax.w);
    
    clampMinMax(validEndColorMin.x, validEndColorMax.x);
    clampMinMax(validEndColorMin.y, validEndColorMax.y);
    clampMinMax(validEndColorMin.z, validEndColorMax.z);
    clampMinMax(validEndColorMin.w, validEndColorMax.w);
    
    clampMinMax(validRotationMin.x, validRotationMax.x);
    clampMinMax(validRotationMin.y, validRotationMax.y);
    clampMinMax(validRotationMin.z, validRotationMax.z);
    
    clampMinMax(validRotationVelocityMin.x, validRotationVelocityMax.x);
    clampMinMax(validRotationVelocityMin.y, validRotationVelocityMax.y);
    clampMinMax(validRotationVelocityMin.z, validRotationVelocityMax.z);
    
    clampMinMax(validLifeTimeMin, validLifeTimeMax);

    // 均等分布乱数生成器
    std::uniform_real_distribution<float> velocityDistX(validVelocityMin.x, validVelocityMax.x);
    std::uniform_real_distribution<float> velocityDistY(validVelocityMin.y, validVelocityMax.y);
    std::uniform_real_distribution<float> velocityDistZ(validVelocityMin.z, validVelocityMax.z);

    std::uniform_real_distribution<float> accelDistX(validAccelMin.x, validAccelMax.x);
    std::uniform_real_distribution<float> accelDistY(validAccelMin.y, validAccelMax.y);
    std::uniform_real_distribution<float> accelDistZ(validAccelMin.z, validAccelMax.z);

    std::uniform_real_distribution<float> startScaleDistX(validStartScaleMin.x, validStartScaleMax.x);
    std::uniform_real_distribution<float> startScaleDistY(validStartScaleMin.y, validStartScaleMax.y);
    std::uniform_real_distribution<float> startScaleDistZ(validStartScaleMin.z, validStartScaleMax.z);

    std::uniform_real_distribution<float> endScaleDistX(validEndScaleMin.x, validEndScaleMax.x);
    std::uniform_real_distribution<float> endScaleDistY(validEndScaleMin.y, validEndScaleMax.y);
    std::uniform_real_distribution<float> endScaleDistZ(validEndScaleMin.z, validEndScaleMax.z);

    std::uniform_real_distribution<float> startColorDistR(validStartColorMin.x, validStartColorMax.x);
    std::uniform_real_distribution<float> startColorDistG(validStartColorMin.y, validStartColorMax.y);
    std::uniform_real_distribution<float> startColorDistB(validStartColorMin.z, validStartColorMax.z);
    std::uniform_real_distribution<float> startColorDistA(validStartColorMin.w, validStartColorMax.w);

    std::uniform_real_distribution<float> endColorDistR(validEndColorMin.x, validEndColorMax.x);
    std::uniform_real_distribution<float> endColorDistG(validEndColorMin.y, validEndColorMax.y);
    std::uniform_real_distribution<float> endColorDistB(validEndColorMin.z, validEndColorMax.z);
    std::uniform_real_distribution<float> endColorDistA(validEndColorMin.w, validEndColorMax.w);

    std::uniform_real_distribution<float> rotationDistX(validRotationMin.x, validRotationMax.x);
    std::uniform_real_distribution<float> rotationDistY(validRotationMin.y, validRotationMax.y);
    std::uniform_real_distribution<float> rotationDistZ(validRotationMin.z, validRotationMax.z);

    std::uniform_real_distribution<float> rotationVelocityDistX(validRotationVelocityMin.x, validRotationVelocityMax.x);
    std::uniform_real_distribution<float> rotationVelocityDistY(validRotationVelocityMin.y, validRotationVelocityMax.y);
    std::uniform_real_distribution<float> rotationVelocityDistZ(validRotationVelocityMin.z, validRotationVelocityMax.z);

    std::uniform_real_distribution<float> lifeTimeDist(validLifeTimeMin, validLifeTimeMax);

    // 指定された数の3Dパーティクルを生成
    for (uint32_t i = 0; i < count; ++i) {
        // パーティクルを直接リストに構築（emplace_backを使用）
        it->second.particles.emplace_back();
        Particle3D& particle = it->second.particles.back();

        // 座標
        particle.position = position;

        // 速度（ランダム）
        particle.velocity.x = velocityDistX(randomEngine_);
        particle.velocity.y = velocityDistY(randomEngine_);
        particle.velocity.z = velocityDistZ(randomEngine_);

        // 加速度（ランダム）
        particle.accel.x = accelDistX(randomEngine_);
        particle.accel.y = accelDistY(randomEngine_);
        particle.accel.z = accelDistZ(randomEngine_);

        // スケール（ランダム）
        particle.startScale.x = startScaleDistX(randomEngine_);
        particle.startScale.y = startScaleDistY(randomEngine_);
        particle.startScale.z = startScaleDistZ(randomEngine_);

        particle.endScale.x = endScaleDistX(randomEngine_);
        particle.endScale.y = endScaleDistY(randomEngine_);
        particle.endScale.z = endScaleDistZ(randomEngine_);

        // Lightningエフェクトの場合は極小スケールから開始
        if (particle.startScale.x < 0.01f && particle.startScale.y < 0.01f && particle.startScale.z < 0.01f) {
            particle.scale = particle.startScale;
        } else {
            // その他のエフェクトは通常のstartScale
            particle.scale = particle.startScale;
        }

        // 色（ランダム）
        particle.startColor.x = startColorDistR(randomEngine_);
        particle.startColor.y = startColorDistG(randomEngine_);
        particle.startColor.z = startColorDistB(randomEngine_);
        particle.startColor.w = startColorDistA(randomEngine_);

        particle.endColor.x = endColorDistR(randomEngine_);
        particle.endColor.y = endColorDistG(randomEngine_);
        particle.endColor.z = endColorDistB(randomEngine_);
        particle.endColor.w = endColorDistA(randomEngine_);

        particle.color = particle.startColor;

        // 回転（ランダム）
        particle.rotation.x = rotationDistX(randomEngine_);
        particle.rotation.y = rotationDistY(randomEngine_);
        particle.rotation.z = rotationDistZ(randomEngine_);

        particle.rotationVelocity.x = rotationVelocityDistX(randomEngine_);
        particle.rotationVelocity.y = rotationVelocityDistY(randomEngine_);
        particle.rotationVelocity.z = rotationVelocityDistZ(randomEngine_);

        // 寿命（ランダム）
        particle.lifeTimeMax = lifeTimeDist(randomEngine_);
        particle.lifeTime = 0.0f;
        
        // 最初のフレームは描画しないフラグ（初期化完了まで非表示）
        particle.isDead = true;

        // Object3Dの作成（正しい引数でInitialize）
        particle.object3d = std::make_unique<Object3d>();
        particle.object3d->Initialize(dxCommon_, spriteCommon_); // SpriteCommonを渡す
        particle.object3d->SetModel(it->second.model.get());
        particle.object3d->SetPosition(particle.position);
        particle.object3d->SetRotation(particle.rotation);
        // スケールを設定
        particle.object3d->SetScale(particle.scale);
        particle.object3d->SetColor(particle.color);
        // カメラはUpdateメソッドで設定される
    }
}
