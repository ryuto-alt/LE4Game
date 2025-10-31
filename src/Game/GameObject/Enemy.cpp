#include "Enemy.h"
#include "Player.h"
#include "NavMesh/NavMesh.h"
#include "NavMesh/NavMeshBuilder.h"
#include "NavMesh/NavMeshHelper.h"
#include "NavMesh/EnemyAI.h"
#include "NavMesh/NavMeshSystem.h"
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

	// 新しいAIシステムを使用する場合
	if (useNewAI_ && enemyAI_ && player_) {
		// EnemyAIを更新
		enemyAI_->Update(deltaTime, player_->GetPosition());

		// AIの状態に基づいてアニメーションを更新
		EnemyState aiState = enemyAI_->GetState();
		switch (aiState) {
			case EnemyState::Patrol:
				if (GetCurrentAnimationName() != "Walk") {
					ChangeAnimation("Walk");
				}
				break;
			case EnemyState::Chase:
				if (GetCurrentAnimationName() != "Run") {
					ChangeAnimation("Run");
				}
				break;
			case EnemyState::Attack:
				if (GetCurrentAnimationName() != "Scream") {
					ChangeAnimation("Scream");
				}
				break;
			default:
				break;
		}

		// AIから位置と回転を取得
		position_ = enemyAI_->GetPosition();
		currentRotationY_ = enemyAI_->GetRotationY();

		// 検知サウンドの更新
		if (aiState == EnemyState::Chase && !isChasing_) {
			// 追跡開始時のサウンド処理
			isChasing_ = true;
		} else if (aiState != EnemyState::Chase && isChasing_) {
			// 追跡終了
			isChasing_ = false;
		}
	}
	// 旧システム（徘徊機能付き）
	else {
		// 代替経路タイマーの更新
		if (alternativeTimer_ > 0.0f) {
			alternativeTimer_ -= deltaTime;
		}

		// プレイヤー検知と追跡 (NavMeshベース)
		if (player_ && navMesh_ && navMesh_->IsValid()) {
			// 視界内にプレイヤーがいるかチェック
			bool playerVisible = IsPlayerInVision();
			Vector3 playerPos = player_->GetPosition();
			Vector3 toPlayer = {
				playerPos.x - position_.x,
				playerPos.y - position_.y,
				playerPos.z - position_.z
			};
			float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);

			// 追跡条件：視界内にいる OR (視界を失って5秒以内 AND 15m以内)
			bool shouldChase = playerVisible ||
			                   (isChasing_ && lostSightTimer_ < LOST_SIGHT_GRACE_PERIOD && distanceToPlayer <= CHASE_RELEASE_DISTANCE);

			if (playerVisible) {
				// プレイヤーが見えている：最後に見た位置を更新
				lastSeenPlayerPosition_ = playerPos;
				lostSightTimer_ = 0.0f;
			} else if (isChasing_) {
				// 視界を失ったがまだ追跡中：タイマーを進める
				lostSightTimer_ += deltaTime;
			}

			if (shouldChase) {
				// プレイヤーを追跡
				if (!isChasing_) {
					ChangeAnimation("Run");
					isChasing_ = true;
					lostSightTimer_ = 0.0f;
				}

				// パス更新タイマーを減算
				pathUpdateTimer_ -= deltaTime;

				// 追跡中は頻繁に経路を更新（0.1秒間隔）
				float chaseUpdateInterval = 0.1f;
				if (pathUpdateTimer_ <= 0.0f) {
					UpdateNavMeshPath();
					pathUpdateTimer_ = chaseUpdateInterval;
				}

				// パスに沿って移動
				if (!currentPath_.empty()) {
					FollowPath();
				}
			} else {
				// プレイヤーが視界外 & 追跡時間切れ - 徘徊モード
				if (isChasing_) {
					// 追跡終了：徘徊状態に
					ChangeAnimation("Walk");
					isChasing_ = false;
					currentPath_.clear();
					currentWaypointIndex_ = 0;
					lostSightTimer_ = 0.0f;
				}

				// 徘徊ロジック
				// パスが空、または目的地に到達した場合、すぐに新しいランダムポイントを選択
				if (currentPath_.empty() || currentWaypointIndex_ >= static_cast<int>(currentPath_.size())) {
					// NavMesh上のランダムな点を取得
					Vector3 randomPoint = GetRandomPatrolPoint();
					float startPos[3] = { position_.x, position_.y, position_.z };
					float endPos[3] = { randomPoint.x, randomPoint.y, randomPoint.z };

					NavMeshPath path;
					if (navMesh_->FindPath(startPos, endPos, path) && path.isValid) {
						currentPath_.clear();
						for (int i = 0; i < path.GetWaypointCount(); ++i) {
							float x, y, z;
							path.GetWaypoint(i, x, y, z);
							currentPath_.push_back({ x, y, z });
						}
						currentWaypointIndex_ = 0;
					}
				}

				// パス更新（移動中でも定期的に再計算して精度向上）
				pathUpdateTimer_ -= deltaTime;
				if (pathUpdateTimer_ <= 0.0f && !currentPath_.empty()) {
					// 現在の目標地点へのパスを再計算
					Vector3 target = currentPath_[currentPath_.size() - 1];
					float startPos[3] = { position_.x, position_.y, position_.z };
					float endPos[3] = { target.x, target.y, target.z };

					NavMeshPath path;
					if (navMesh_->FindPath(startPos, endPos, path) && path.isValid) {
						currentPath_.clear();
						for (int i = 0; i < path.GetWaypointCount(); ++i) {
							float x, y, z;
							path.GetWaypoint(i, x, y, z);
							currentPath_.push_back({ x, y, z });
						}
						currentWaypointIndex_ = 0;
					}

					pathUpdateTimer_ = pathUpdateInterval_;
				}

				// パスに沿って移動
				if (!currentPath_.empty()) {
					FollowPath();
				}
			}
		}

		// スタック検出と回避処理
		CheckAndHandleStuck();
	}

	// アニメーションの更新
	UpdateAnimation();

	// 足音の更新
	UpdateFootstepAudio();

	// 検知サウンドの更新
	UpdateDetectionSound();

	// オブジェクトの位置と回転を更新
	if (object3d_) {
		object3d_->SetPosition(position_);
		object3d_->SetRotation(Vector3{0.0f, currentRotationY_, 0.0f});
		object3d_->Update();
	}

	// 衝突応答処理（NavMeshで経路制御しているため無効化）
	// HandleCollisionResponse();
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
	// 新しいAIシステムにも位置を設定
	if (enemyAI_) {
		enemyAI_->SetPosition(position_);
	}
}

void Enemy::SetNavMeshSystem(NavMeshSystem* navMeshSystem) {
	// 新しいAIシステムを作成・初期化
	if (!enemyAI_) {
		enemyAI_ = std::make_unique<EnemyAI>();
	}

	// NavMeshSystemを設定して初期化
	enemyAI_->Initialize(navMeshSystem);
	enemyAI_->SetPosition(position_);

	// AI設定を適用
	enemyAI_->SetDetectionRange(aiConfig_.aggressiveness);
	enemyAI_->SetMoveSpeed(0.025f + (aiConfig_.mobility * 0.0225f));
	enemyAI_->SetUpdateRate(2.0f - (aiConfig_.intelligence * 0.19f));

	// 徘徊モードを有効化
	enemyAI_->EnablePatrolMode(true);
	enemyAI_->SetPatrolRadius(50.0f);  // 50m範囲で徘徊
	enemyAI_->SetPatrolCenter(position_);  // 現在位置を中心に

	// 新しいAIを使用
	useNewAI_ = true;
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
	// 現在のパスをクリア
	currentPath_.clear();
	currentWaypointIndex_ = 0;

	// 追跡中の場合はパスを再計算
	if (isChasing_ && player_) {
		UpdateNavMeshPath();
	}

	// 回避フラグをリセット
	isRecoveringFromStuck_ = false;
	stuckRecoveryAttempts_ = 0;
}

// ランダムな徘徊ポイントを取得（旧NavMesh用）
Vector3 Enemy::GetRandomPatrolPoint() {
	static std::random_device rd;
	static std::mt19937 gen(rd());

	const float PATROL_RADIUS = 50.0f;  // 50m範囲
	const int MAX_ATTEMPTS = 20;

	for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
		// ランダムな角度と距離
		std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
		std::uniform_real_distribution<float> radiusDist(PATROL_RADIUS * 0.3f, PATROL_RADIUS);

		float angle = angleDist(gen);
		float distance = radiusDist(gen);

		// 候補位置を計算
		Vector3 candidatePoint = {
			position_.x + distance * std::cos(angle),
			position_.y,
			position_.z + distance * std::sin(angle)
		};

		// パスが見つかるか試す（これで有効な点かどうかをチェック）
		if (navMesh_ && navMesh_->IsValid()) {
			float startPos[3] = { position_.x, position_.y, position_.z };
			float endPos[3] = { candidatePoint.x, candidatePoint.y, candidatePoint.z };

			NavMeshPath testPath;
			if (navMesh_->FindPath(startPos, endPos, testPath) && testPath.isValid && testPath.GetWaypointCount() > 0) {
				// パスが見つかった場合、最後のウェイポイントを返す
				float x, y, z;
				testPath.GetWaypoint(testPath.GetWaypointCount() - 1, x, y, z);
				return { x, y, z };
			}
		}
	}

	// 失敗した場合は現在位置から少し離れた位置を返す
	std::uniform_real_distribution<float> smallDist(5.0f, 10.0f);
	std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
	float angle = angleDist(gen);
	float distance = smallDist(gen);

	return {
		position_.x + distance * std::cos(angle),
		position_.y,
		position_.z + distance * std::sin(angle)
	};
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
