#include "Enemy.h"
#include "Player.h"
#include "NavMesh/NavMesh.h"
#include "NavMesh/NavMeshBuilder.h"
#include "NavMesh/NavMeshHelper.h"
#include <cmath>
#include "Collision/AABBCollision.h"
#include "Collision/CollisionHelper.h"
#include "DetourNavMeshQuery.h"

Enemy::Enemy() = default;

Enemy::~Enemy() {
}

void Enemy::ApplyAIConfig() {
	// Intelligence: 0.0~10.0 → パス更新間隔 2.0~0.1秒
	// 5.0で0.5秒（デフォルト）
	pathUpdateInterval_ = 2.0f - (aiConfig_.intelligence * 0.19f);

	// Aggressiveness: 0.0~10.0 → 検知範囲 0.0~10.0m
	// aggressivenessの値をそのまま検知距離（メートル）として使用
	detectionRange_ = aiConfig_.aggressiveness;

	// Mobility: 0.0~10.0 → 移動速度 0.025~0.25
	// 5.0で0.125（デフォルト）
	moveSpeed_ = 0.025f + (aiConfig_.mobility * 0.0225f);
}

void Enemy::Initialize(Camera* camera, const EnemyAIConfig& aiConfig) {
	// AI設定を適用
	aiConfig_ = aiConfig;
	aiConfig_.Clamp();
	ApplyAIConfig();

	UnoEngine* engine = UnoEngine::GetInstance();

	// Playerと全く同じ方法でアニメーションモデルを読み込む
	animatedModel_ = engine->CreateAnim();
	animatedModel_->LoadFromFile("Resources/Models/Enemy/EnemyWalk", "EnemyWalk.gltf");

	// Playerと同じパターン: 読み込んだGLTFのアニメーションを取得して登録
	Animation walkAnim = animatedModel_->GetAnimationPlayer().GetAnimation();
	animatedModel_->AddAnimation("Walk", walkAnim);

	// RunアニメーションとScreamアニメーションも読み込む
	std::unique_ptr<AnimatedModel> runModel = engine->CreateAnim();
	runModel->LoadFromFile("Resources/Models/Enemy/EnemyRun", "EnemyRun.gltf");
	Animation runAnim = runModel->GetAnimationPlayer().GetAnimation();
	animatedModel_->AddAnimation("Run", runAnim);

	std::unique_ptr<AnimatedModel> screamModel = engine->CreateAnim();
	screamModel->LoadFromFile("Resources/Models/Enemy/EnemyScream", "EnemyScream.gltf");
	Animation screamAnim = screamModel->GetAnimationPlayer().GetAnimation();
	animatedModel_->AddAnimation("Scream", screamAnim);

	// Playerと同じ: アニメーションを変更して再生
	animatedModel_->ChangeAnimation("Walk");
	animatedModel_->PlayAnimation();

	// Object3Dの作成 - Playerと全く同じ順序
	object3d_ = engine->CreateObj3();
	object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
	object3d_->SetAnimatedModel(animatedModel_.get());
	object3d_->SetPosition(position_);
	object3d_->SetScale(Vector3{3.0f, 3.0f, 3.0f});
	object3d_->SetRotation(Vector3{0.0f, 3.14f, 0.0f}); // Playerと同じY軸180度回転
	object3d_->SetEnableLighting(true);
	object3d_->SetEnableAnimation(true);
	object3d_->SetCamera(camera);

	// 環境マップを無効化（Playerと同じ）
	object3d_->EnableEnv(false);

	// コリジョン設定（有効化）
	auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
	if (collisionManager && object3d_ && animatedModel_) {
		Collision::AABB enemyAABB = Collision::AABBExtractor::ExtractFromAnimatedModel(animatedModel_.get());
		collisionManager->RegisterObject(object3d_.get(), enemyAABB, true, "Enemy");  // trueで有効化
	}

	// PBRマテリアルでない場合、強制的にPBRを有効化
	if (animatedModel_) {
		const MaterialData& material = animatedModel_->GetMaterial();
		if (!material.isPBR) {
			MaterialData& mutableMaterial = const_cast<MaterialData&>(animatedModel_->GetMaterial());
			mutableMaterial.isPBR = true;
			mutableMaterial.baseColorFactor = { 0.8f, 0.8f, 0.8f, 1.0f };
			mutableMaterial.metallicFactor = 0.0f;
			mutableMaterial.roughnessFactor = 0.8f;
			mutableMaterial.emissiveFactor = { 0.0f, 0.0f, 0.0f };
			mutableMaterial.alphaMode = "OPAQUE";
			mutableMaterial.doubleSided = false;
			object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
		}
	}

	// 3D空間オーディオの初期化
	footstepSource1_ = std::make_unique<SpatialAudioSource>();
	footstepSource1_->Initialize("Resources/Audio/EnemyWalk_1.mp3", position_);
	footstepSource1_->SetVolume(0.8f);
	footstepSource1_->SetMaxDistance(30.0f);
	footstepSource1_->SetMinDistance(1.0f);

	footstepSource2_ = std::make_unique<SpatialAudioSource>();
	footstepSource2_->Initialize("Resources/Audio/EnemyWalk_2.mp3", position_);
	footstepSource2_->SetVolume(0.8f);
	footstepSource2_->SetMaxDistance(30.0f);
	footstepSource2_->SetMinDistance(1.0f);
}

void Enemy::Update() {
	const float deltaTime = 1.0f / 60.0f; // 60 FPS想定

	// 代替経路タイマーの更新
	if (alternativeTimer_ > 0.0f) {
		alternativeTimer_ -= deltaTime;
	}

	// プレイヤー検知と追跡 (NavMeshベース)
	if (player_ && navMesh_ && navMesh_->IsValid()) {
		Vector3 playerPos = player_->GetPosition();
		Vector3 toPlayer = {
			playerPos.x - position_.x,
			0.0f,
			playerPos.z - position_.z
		};

		float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

		// プレイヤーが検知範囲内にいるかチェック
		if (distanceToPlayer < detectionRange_) {
			if (!isChasing_) {
				// 追跡開始：Runアニメーションに変更
				ChangeAnimation("Run");
				isChasing_ = true;
			}

			// パス更新タイマーを減算
			pathUpdateTimer_ -= deltaTime;

			// 定期的にパスを更新
			if (pathUpdateTimer_ <= 0.0f) {
				UpdateNavMeshPath();
				pathUpdateTimer_ = pathUpdateInterval_;
			}

			// パスに沿って移動
			if (!currentPath_.empty()) {
				FollowPath();
			}
		} else {
			if (isChasing_) {
				// 追跡終了：Walkアニメーションに戻す
				ChangeAnimation("Walk");
				isChasing_ = false;
				currentPath_.clear();
				currentWaypointIndex_ = 0;
			}
		}
	} else if (player_) {
		// NavMeshがない場合は従来の簡易追跡
		Vector3 playerPos = player_->GetPosition();
		Vector3 toPlayer = {
			playerPos.x - position_.x,
			0.0f,
			playerPos.z - position_.z
		};

		float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

		if (distanceToPlayer < detectionRange_) {
			if (!isChasing_) {
				ChangeAnimation("Run");
				isChasing_ = true;
			}

			if (distanceToPlayer > 1.0f) {
				float invLength = 1.0f / distanceToPlayer;
				toPlayer.x *= invLength;
				toPlayer.z *= invLength;

				position_.x += toPlayer.x * moveSpeed_;
				position_.z += toPlayer.z * moveSpeed_;

				currentRotationY_ = std::atan2(toPlayer.x, toPlayer.z);
			}
		} else {
			if (isChasing_) {
				ChangeAnimation("Walk");
				isChasing_ = false;
			}
		}
	}

	// アニメーションの更新
	UpdateAnimation();

	// 足音の更新
	UpdateFootstepAudio();

	// オブジェクトの位置と回転を更新
	if (object3d_) {
		object3d_->SetPosition(position_);
		object3d_->SetRotation(Vector3{0.0f, currentRotationY_, 0.0f});
		object3d_->Update();
	}

	// 衝突応答処理
	HandleCollisionResponse();
}

void Enemy::UpdateAnimation() {
	float deltaTime = 1.0f / 60.0f; // 60 FPS想定

	if (animatedModel_) {
		// Playerと同じパターン
		if (!animationPaused_) {
			animatedModel_->Update(deltaTime);
		} else {
			animatedModel_->Update(0.0f);
		}
	}

	// ブレンドタイマーの更新
	if (isBlending_) {
		blendTimer_ += deltaTime;
		if (blendTimer_ >= BLEND_DURATION) {
			isBlending_ = false;
			blendTimer_ = 0.0f;
		}
	}
}

void Enemy::UpdateFootstepAudio() {
	// 足音を無効化
	return;

	// WalkまたはRunアニメーション中のみ足音を再生
	std::string currentAnim = GetCurrentAnimationName();
	if (currentAnim != "Walk" && currentAnim != "Run") {
		return;
	}

	// アニメーションが停止している場合はスキップ
	if (animationPaused_ || !animatedModel_) {
		return;
	}

	// リスナーが設定されていない場合はスキップ
	if (!audioListener_) {
		return;
	}

	// アニメーションの現在時刻を取得
	float currentTime = animatedModel_->GetAnimationPlayer().GetTime();

	// 前回の時刻から一定時間経過したかチェック
	if (currentTime - lastAnimationTime_ >= FOOTSTEP_INTERVAL) {
		// 3D位置を更新して再生
		if (footstepSource1_ && footstepSource2_) {
			// リスナーの位置と向きを取得
			Vector3 listenerPos = audioListener_->GetPosition();
			Vector3 listenerForward = audioListener_->GetForward();

			// 交互に足音を再生
			if (useFootstep1_) {
				footstepSource1_->SetPosition(position_);
				footstepSource1_->Update(listenerPos, listenerForward);
				footstepSource1_->Play(false);  // ループなし
			} else {
				footstepSource2_->SetPosition(position_);
				footstepSource2_->Update(listenerPos, listenerForward);
				footstepSource2_->Play(false);  // ループなし
			}

			// 次回は別の足音を使用
			useFootstep1_ = !useFootstep1_;
		}

		lastAnimationTime_ = currentTime;
	}

	// アニメーションがループした場合のリセット
	if (currentTime < lastAnimationTime_) {
		lastAnimationTime_ = 0.0f;
	}
}

void Enemy::Draw() {
	if (object3d_) {
		object3d_->Draw();
	}
}


void Enemy::Finalize() {
	if (animatedModel_) {
		animatedModel_.reset();
	}
	if (object3d_) {
		object3d_.reset();
	}
}

void Enemy::SetPosition(const Vector3& position) {
	position_ = position;
	if (object3d_) {
		object3d_->SetPosition(position_);
	}
}

void Enemy::PauseAnimation() {
	animationPaused_ = true;
	if (animatedModel_) {
		animatedModel_->PauseAnimation();
	}
}

void Enemy::PlayAnimation() {
	animationPaused_ = false;
	if (animatedModel_) {
		animatedModel_->PlayAnimation();
	}
}

void Enemy::ResetAnimation() {
	if (animatedModel_) {
		std::string currentAnim = GetCurrentAnimationName();
		if (!currentAnim.empty()) {
			animatedModel_->ChangeAnimation(currentAnim);
		}
	}
}

std::string Enemy::GetCurrentAnimationName() const {
	if (animatedModel_) {
		return animatedModel_->GetCurrentAnimationName();
	}
	return "";
}

void Enemy::SetDirectionalLight(DirectionalLight* light) {
	if (object3d_ && light) {
		object3d_->SetDirectionalLight(*light);
	}
}

void Enemy::SetSpotLight(SpotLight* light) {
	if (object3d_ && light) {
		object3d_->SetSpotLight(*light);
	}
}

void Enemy::EnableEnv(bool enable) {
	if (object3d_) {
		object3d_->EnableEnv(enable);
	}
}

bool Enemy::IsEnvEnabled() const {
	if (object3d_) {
		return object3d_->IsEnvEnabled();
	}
	return false;
}

void Enemy::SetEnvTex(const std::string& texturePath) {
	if (object3d_) {
		Object3d::SetEnvTex(texturePath);
	}
}

void Enemy::SetCamera(Camera* camera) {
	if (object3d_) {
		object3d_->SetCamera(camera);
	}
}

void Enemy::ChangeAnimation(const std::string& animationName) {
	if (animatedModel_) {
		animatedModel_->TransitionToAnimation(animationName, BLEND_DURATION);
		isBlending_ = true;
		blendTimer_ = 0.0f;
	}
}

void Enemy::SetIntelligence(float value) {
	aiConfig_.intelligence = std::clamp(value, 0.0f, 10.0f);
	ApplyAIConfig();
}

void Enemy::SetAggressiveness(float value) {
	aiConfig_.aggressiveness = std::clamp(value, 0.0f, 100.0f);
	ApplyAIConfig();
}

void Enemy::SetMobility(float value) {
	aiConfig_.mobility = std::clamp(value, 0.0f, 10.0f);
	ApplyAIConfig();
}

void Enemy::SetAIConfig(const EnemyAIConfig& config) {
	aiConfig_ = config;
	aiConfig_.Clamp();
	ApplyAIConfig();
}

// 指定位置に壁があるかチェック
bool Enemy::CheckWallAt(const Vector3& checkPosition) {
	auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
	if (!collisionManager || !object3d_) {
		return false;
	}

	auto enemyColObj = collisionManager->FindCollisionObject(object3d_.get());
	if (!enemyColObj) {
		return false;
	}

	// チェック位置が障害物内にあるか
	const float checkRadius = 1.0f;

	for (const auto& colObj : collisionManager->GetCollisionObjects()) {
		if (colObj.get() == enemyColObj.get()) continue;
		if (!colObj->IsEnabled()) continue;
		if (colObj->GetName() == "Player") continue;

		const Collision::AABB& obstacleAABB = colObj->GetWorldAABB();

		// チェック位置が障害物のAABB内にあるか
		if (checkPosition.x >= obstacleAABB.min.x - checkRadius &&
		    checkPosition.x <= obstacleAABB.max.x + checkRadius &&
		    checkPosition.z >= obstacleAABB.min.z - checkRadius &&
		    checkPosition.z <= obstacleAABB.max.z + checkRadius) {
			return true;
		}
	}

	return false;
}

// 衝突応答処理
void Enemy::HandleCollisionResponse() {
	Collision::CollisionHelper::HandleAABBPushout(
		position_,
		object3d_.get(),
		{"Player"}  // wallとの当たり判定は無効化
	);
}

// NavMeshパスを更新
void Enemy::UpdateNavMeshPath() {
	if (!navMesh_ || !player_) {
		return;
	}

	Vector3 playerPos = player_->GetPosition();
	float startPos[3] = { position_.x, position_.y, position_.z };
	float endPos[3] = { playerPos.x, playerPos.y, playerPos.z };

	NavMeshPath path;
	if (navMesh_->FindPath(startPos, endPos, path) && path.isValid) {
		// パスをVector3のリストに変換
		currentPath_.clear();
		for (int i = 0; i < path.GetWaypointCount(); ++i) {
			float x, y, z;
			path.GetWaypoint(i, x, y, z);
			currentPath_.push_back({ x, y, z });
		}
		currentWaypointIndex_ = 0;
	} else {
		currentPath_.clear();
		currentWaypointIndex_ = 0;
	}
}

// パスに沿って移動（先読みで滑らかに）
void Enemy::FollowPath() {
	NavMeshHelper::FollowPath(
		position_,
		currentRotationY_,
		currentSpeed_,
		currentPath_,
		currentWaypointIndex_,
		moveSpeed_,
		navMesh_,
		&isAtCorner_,
		&cornerSlowdownFactor_
	);
}
