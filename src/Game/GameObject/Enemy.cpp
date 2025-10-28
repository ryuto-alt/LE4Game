#include "Enemy.h"
#include "Player.h"
#include "NavMesh/NavMesh.h"
#include "NavMesh/NavMeshBuilder.h"
#include "NavMesh/NavMeshHelper.h"
#include <cmath>
#include <random>
#include "Collision/AABBCollision.h"
#include "Collision/CollisionHelper.h"
#include "DetourNavMeshQuery.h"
#include "imgui.h"

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

	// 検知サウンドの初期化
	detectionSound_ = std::make_unique<SpatialAudioSource>();
	detectionSound_->Initialize("Resources/Audio/enemysound.mp3", position_);
	detectionSound_->SetVolume(2.2f);  // 大き目の音量
	detectionSound_->SetMaxDistance(40.0f);
	detectionSound_->SetMinDistance(1.0f);
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

		// 検知条件：視界内にいるか、または追跡中の場合は追跡解除距離以内
		bool shouldChase = false;
		if (isChasing_) {
			// 追跡中は25m以上離れないと解除しない
			shouldChase = (distanceToPlayer < CHASE_RELEASE_DISTANCE);
		} else {
			// 探索中は視界内（前方30m以内）にいるか確認
			shouldChase = IsPlayerInVision();
		}

		if (shouldChase) {
			// 探索モードから追跡モードに切り替え
			if (isExploring_) {
				isExploring_ = false;
				explorationIdleTimer_ = 0.0f;
			}

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
			// プレイヤーが検知範囲外
			if (isChasing_) {
				// 追跡終了：探索モードに切り替え
				ChangeAnimation("Walk");
				isChasing_ = false;
				isExploring_ = true;
				currentPath_.clear();
				currentWaypointIndex_ = 0;
				// すぐに新しい探索ポイントを生成
				GenerateRandomExplorationPoint();
			} else if (isExploring_) {
				// 探索モード中
				UpdateExploration();
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
				isExploring_ = false;
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
				isExploring_ = true;
			}
		}
	} else if (navMesh_ && navMesh_->IsValid()) {
		// プレイヤーが設定されていない場合でも探索モード
		if (isExploring_) {
			UpdateExploration();
		}
	}

	// アニメーションの更新
	UpdateAnimation();

	// 足音の更新
	UpdateFootstepAudio();

	// 検知サウンドの更新
	UpdateDetectionSound();

	// スタック検出と回避処理
	CheckAndHandleStuck();

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

void Enemy::UpdateDetectionSound() {
	// プレイヤーとリスナーが設定されていない場合はスキップ
	if (!player_ || !audioListener_ || !detectionSound_) {
		return;
	}

	// リスナーの位置と向きを取得（毎フレーム更新）
	Vector3 listenerPos = audioListener_->GetPosition();
	Vector3 listenerForward = audioListener_->GetForward();

	// 3D位置を常に更新
	detectionSound_->SetPosition(position_);
	detectionSound_->Update(listenerPos, listenerForward);

	// 現在の時刻を取得（秒単位）
	static float totalTime = 0.0f;
	totalTime += 1.0f / 60.0f;

	// 再生状態を確認
	bool currentlyPlaying = detectionSound_->IsPlaying();

	// 前フレームで再生中だったが今は停止している場合、終了時刻を記録
	if (isDetectionSoundPlaying_ && !currentlyPlaying) {
		lastDetectionSoundEndTime_ = totalTime;
		isDetectionSoundPlaying_ = false;
	}

	// 現在再生中の場合、フラグを更新
	if (currentlyPlaying) {
		isDetectionSoundPlaying_ = true;
	}

	// プレイヤーとの距離を計算
	Vector3 playerPos = player_->GetPosition();
	Vector3 toPlayer = {
		playerPos.x - position_.x,
		0.0f,
		playerPos.z - position_.z
	};
	float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

	// 20m以内にプレイヤーがいるかチェック
	if (distanceToPlayer <= DETECTION_SOUND_RANGE) {
		// 再生中でなく、かつクールタイムが経過している場合のみ再生
		if (!isDetectionSoundPlaying_ &&
		    totalTime - lastDetectionSoundEndTime_ >= DETECTION_SOUND_COOLDOWN) {
			// 再生
			detectionSound_->Play(false);  // ループなし
			isDetectionSoundPlaying_ = true;
		}
	}
}

void Enemy::Draw() {
	if (object3d_) {
		object3d_->Draw();
	}

	// デバッグ用視界描画
	if (debugDrawVision_) {
		DrawDebugVision();
	}
}

void Enemy::DrawDebugVision() {
	if (!player_) {
		return;
	}

	// ImGuiウィンドウで視界情報を表示
	ImGui::Begin("Enemy Vision Debug");

	// プレイヤーとの距離を計算
	Vector3 playerPos = player_->GetPosition();
	Vector3 toPlayer = {
		playerPos.x - position_.x,
		0.0f,
		playerPos.z - position_.z
	};
	float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

	// Enemyの状態
	ImGui::Text("Enemy Status:");
	ImGui::Text("  Position: (%.2f, %.2f, %.2f)", position_.x, position_.y, position_.z);
	ImGui::Text("  Rotation Y: %.2f degrees", currentRotationY_ * (180.0f / 3.14159f));
	ImGui::Text("  Is Chasing: %s", isChasing_ ? "YES" : "NO");
	ImGui::Text("  Is Exploring: %s", isExploring_ ? "YES" : "NO");

	ImGui::Separator();

	// プレイヤーとの関係
	ImGui::Text("Player Relation:");
	ImGui::Text("  Distance: %.2f m", distanceToPlayer);
	ImGui::Text("  Vision Detection Distance: %.2f m", VISION_DETECTION_DISTANCE);
	ImGui::Text("  Chase Release Distance: %.2f m", CHASE_RELEASE_DISTANCE);

	// 視界チェック結果
	bool inVision = IsPlayerInVision();
	ImGui::Separator();
	ImGui::Text("Vision Check:");

	if (distanceToPlayer <= VISION_DETECTION_DISTANCE) {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "  Distance: WITHIN detection range (15m)");

		// 角度計算
		if (distanceToPlayer > 0.01f) {
			float invLength = 1.0f / distanceToPlayer;
			Vector3 toPlayerNorm = {
				toPlayer.x * invLength,
				0.0f,
				toPlayer.z * invLength
			};

			Vector3 forward = {
				std::sin(currentRotationY_),
				0.0f,
				std::cos(currentRotationY_)
			};

			float dotProduct = toPlayerNorm.x * forward.x + toPlayerNorm.z * forward.z;
			if (dotProduct > 1.0f) dotProduct = 1.0f;
			if (dotProduct < -1.0f) dotProduct = -1.0f;

			float angleInRadians = std::acos(dotProduct);
			float angleInDegrees = angleInRadians * (180.0f / 3.14159f);

			ImGui::Text("  Angle to Player: %.2f degrees", angleInDegrees);
			ImGui::Text("  Vision Angle: %.2f degrees", VISION_ANGLE);

			if (angleInDegrees <= VISION_ANGLE) {
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "  Angle: WITHIN vision cone");
			} else {
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "  Angle: OUTSIDE vision cone");
			}
		}
	} else {
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "  Distance: OUTSIDE detection range (>15m)");
	}

	ImGui::Separator();
	if (inVision) {
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "PLAYER DETECTED!");
	} else {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Player not in vision");
	}

	// 追跡中の場合、追跡解除までの距離
	if (isChasing_) {
		ImGui::Separator();
		float distanceToRelease = CHASE_RELEASE_DISTANCE - distanceToPlayer;
		if (distanceToRelease > 0) {
			ImGui::Text("Distance until chase release: %.2f m", distanceToRelease);
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Will release chase now!");
		}
	}

	ImGui::End();
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

// ランダムな探索ポイントを生成
void Enemy::GenerateRandomExplorationPoint() {
	if (!navMesh_ || !navMesh_->IsValid()) {
		return;
	}

	// プレイヤーが設定されていない場合は従来の動作
	if (!player_) {
		// ランダムジェネレーターの初期化
		static std::random_device rd;
		static std::mt19937 gen(rd());
		std::uniform_real_distribution<float> distX(-20.0f, 20.0f);
		std::uniform_real_distribution<float> distZ(-20.0f, 20.0f);

		Vector3 randomOffset = {
			distX(gen),
			0.0f,
			distZ(gen)
		};

		Vector3 targetPos = {
			position_.x + randomOffset.x,
			position_.y,
			position_.z + randomOffset.z
		};

		float startPos[3] = { position_.x, position_.y, position_.z };
		float endPos[3] = { targetPos.x, targetPos.y, targetPos.z };

		NavMeshPath path;
		if (navMesh_->FindPath(startPos, endPos, path) && path.isValid) {
			if (path.GetWaypointCount() > 0) {
				float x, y, z;
				path.GetWaypoint(path.GetWaypointCount() - 1, x, y, z);
				explorationTarget_ = { x, y, z };

				currentPath_.clear();
				for (int i = 0; i < path.GetWaypointCount(); ++i) {
					path.GetWaypoint(i, x, y, z);
					currentPath_.push_back({ x, y, z });
				}
				currentWaypointIndex_ = 0;
			}
		}
		return;
	}

	// プレイヤーの位置を基準に探索範囲を設定
	Vector3 playerPos = player_->GetPosition();

	// ランダムジェネレーターの初期化
	static std::random_device rd;
	static std::mt19937 gen(rd());

	// 最大試行回数
	const int maxAttempts = 10;

	for (int attempt = 0; attempt < maxAttempts; ++attempt) {
		// プレイヤーからランダムな方向と距離でポイントを生成
		std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
		std::uniform_real_distribution<float> distanceDist(EXPLORATION_MIN_DISTANCE, EXPLORATION_MAX_DISTANCE);

		float angle = angleDist(gen);
		float distance = distanceDist(gen);

		// プレイヤーから一定距離離れた位置を計算
		Vector3 targetPos = {
			playerPos.x + std::cos(angle) * distance,
			playerPos.y,
			playerPos.z + std::sin(angle) * distance
		};

		// NavMesh上の最も近い有効な位置を取得
		float startPos[3] = { position_.x, position_.y, position_.z };
		float endPos[3] = { targetPos.x, targetPos.y, targetPos.z };

		NavMeshPath path;
		if (navMesh_->FindPath(startPos, endPos, path) && path.isValid) {
			// パスが見つかった場合、最終地点を探索目標にする
			if (path.GetWaypointCount() > 0) {
				float x, y, z;
				path.GetWaypoint(path.GetWaypointCount() - 1, x, y, z);
				explorationTarget_ = { x, y, z };

				// 生成されたポイントがプレイヤーから適切な距離かチェック
				Vector3 toPlayer = {
					playerPos.x - explorationTarget_.x,
					0.0f,
					playerPos.z - explorationTarget_.z
				};
				float distToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

				// 適切な距離範囲内なら採用
				if (distToPlayer >= EXPLORATION_MIN_DISTANCE && distToPlayer <= EXPLORATION_MAX_DISTANCE) {
					// パスを設定
					currentPath_.clear();
					for (int i = 0; i < path.GetWaypointCount(); ++i) {
						path.GetWaypoint(i, x, y, z);
						currentPath_.push_back({ x, y, z });
					}
					currentWaypointIndex_ = 0;
					return;
				}
			}
		}
	}

	// 適切なポイントが見つからなかった場合、現在位置近くにランダムポイントを生成
	std::uniform_real_distribution<float> nearbyDist(-EXPLORATION_RANGE, EXPLORATION_RANGE);
	Vector3 targetPos = {
		position_.x + nearbyDist(gen),
		position_.y,
		position_.z + nearbyDist(gen)
	};

	float startPos[3] = { position_.x, position_.y, position_.z };
	float endPos[3] = { targetPos.x, targetPos.y, targetPos.z };

	NavMeshPath path;
	if (navMesh_->FindPath(startPos, endPos, path) && path.isValid) {
		if (path.GetWaypointCount() > 0) {
			float x, y, z;
			path.GetWaypoint(path.GetWaypointCount() - 1, x, y, z);
			explorationTarget_ = { x, y, z };

			currentPath_.clear();
			for (int i = 0; i < path.GetWaypointCount(); ++i) {
				path.GetWaypoint(i, x, y, z);
				currentPath_.push_back({ x, y, z });
			}
			currentWaypointIndex_ = 0;
		}
	}
}

// 探索フェーズの更新
void Enemy::UpdateExploration() {
	const float deltaTime = 1.0f / 60.0f;

	// 待機中の場合
	if (explorationIdleTimer_ > 0.0f) {
		explorationIdleTimer_ -= deltaTime;
		if (explorationIdleTimer_ <= 0.0f) {
			// 待機終了、新しい目標を生成
			GenerateRandomExplorationPoint();
		}
		return;
	}

	// 目標地点に到達したかチェック
	Vector3 toTarget = {
		explorationTarget_.x - position_.x,
		0.0f,
		explorationTarget_.z - position_.z
	};
	float distanceToTarget = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

	if (distanceToTarget < EXPLORATION_ARRIVAL_THRESHOLD) {
		// 到達したので待機状態に入る
		explorationIdleTimer_ = EXPLORATION_IDLE_TIME;
		currentPath_.clear();
		currentWaypointIndex_ = 0;
		return;
	}

	// パスに沿って移動
	if (!currentPath_.empty()) {
		FollowPath();
	} else {
		// パスがない場合、新しい目標を生成
		GenerateRandomExplorationPoint();
	}
}

// スタック検出と処理
void Enemy::CheckAndHandleStuck() {
	const float deltaTime = 1.0f / 60.0f;

	// スタック回避中は特別な処理
	if (isRecoveringFromStuck_) {
		RecoverFromStuck();
		return;
	}

	// 現在位置と前フレームの位置の差を計算
	Vector3 movement = {
		position_.x - previousPosition_.x,
		0.0f,
		position_.z - previousPosition_.z
	};
	float movementDistance = std::sqrt(movement.x * movement.x + movement.z * movement.z);

	// 移動量が閾値以下の場合、スタックタイマーを増加
	if (movementDistance < STUCK_DISTANCE_THRESHOLD) {
		stuckTimer_ += deltaTime;

		// 一定時間スタックしている場合、回避処理を開始
		if (stuckTimer_ >= STUCK_DETECTION_TIME) {
			isRecoveringFromStuck_ = true;
			stuckTimer_ = 0.0f;

			// 現在のパスをクリア
			currentPath_.clear();
			currentWaypointIndex_ = 0;
		}
	} else {
		// 正常に移動している場合、タイマーをリセット
		stuckTimer_ = 0.0f;
	}

	// 前フレームの位置を更新
	previousPosition_ = position_;
}

// スタックから回復
void Enemy::RecoverFromStuck() {
	// 新しい探索ポイントを強制的に生成
	if (isExploring_) {
		GenerateRandomExplorationPoint();
	} else if (isChasing_ && player_) {
		// 追跡中の場合はパスを再計算
		UpdateNavMeshPath();
	}

	// 回避フラグをリセット
	isRecoveringFromStuck_ = false;
}

// プレイヤーが視界内にいるかチェック
bool Enemy::IsPlayerInVision() {
	if (!player_) {
		return false;
	}

	// プレイヤーとの距離を計算
	Vector3 playerPos = player_->GetPosition();
	Vector3 toPlayer = {
		playerPos.x - position_.x,
		0.0f,
		playerPos.z - position_.z
	};
	float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

	// 視界検知距離外なら検知しない（15m以内のみ検知）
	if (distanceToPlayer > VISION_DETECTION_DISTANCE) {
		return false;
	}

	// プレイヤーへの方向ベクトルを正規化
	if (distanceToPlayer < 0.01f) {
		// 距離が極端に小さい場合は常に視界内とする
		return true;
	}

	float invLength = 1.0f / distanceToPlayer;
	Vector3 toPlayerNorm = {
		toPlayer.x * invLength,
		0.0f,
		toPlayer.z * invLength
	};

	// Enemyの向きベクトルを計算（Y軸回転）
	Vector3 forward = {
		std::sin(currentRotationY_),
		0.0f,
		std::cos(currentRotationY_)
	};

	// 内積で角度を計算（-1.0 ~ 1.0）
	float dotProduct = toPlayerNorm.x * forward.x + toPlayerNorm.z * forward.z;

	// 内積の範囲をクランプ（数値誤差対策）
	if (dotProduct > 1.0f) dotProduct = 1.0f;
	if (dotProduct < -1.0f) dotProduct = -1.0f;

	// 角度を計算（ラジアンから度に変換）
	float angleInRadians = std::acos(dotProduct);
	float angleInDegrees = angleInRadians * (180.0f / 3.14159f);

	// 視野角以内ならtrue（Enemyの前方±60度、合計120度の視野）
	return angleInDegrees <= VISION_ANGLE;
}
