#include "Enemy.h"
#include "Player.h"
#include "NavMesh/NavMesh.h"
#include "NavMesh/NavMeshBuilder.h"
#include "imgui.h"
#include <numbers>
#include <cmath>
#include "Collision/AABBCollision.h"
#include "DetourNavMeshQuery.h"

Enemy::Enemy()
	: position_({0.0f, 0.0f, 0.0f})
	, currentRotationY_(0.0f)
	, targetRotationY_(0.0f)
	, currentSpeed_(0.0f)
	, animationPaused_(false)
	, isBlending_(false)
	, blendTimer_(0.0f)
	, animationEnabled_(true)
	, currentAnimationIndex_(0)
	, player_(nullptr)
	, detectionRange_(100.0f)
	, moveSpeed_(0.125f)  
	, isChasing_(false)
	, avoidanceRadius_(5.0f)
	, alternativeTimer_(0.0f)
	, navMesh_(nullptr)
	, currentWaypointIndex_(0)
	, pathUpdateTimer_(0.0f)
	, isAtCorner_(false)
	, cornerSlowdownFactor_(1.0f)
	, audioListener_(nullptr)
	, useFootstep1_(true)
	, lastAnimationTime_(0.0f) {
}

Enemy::~Enemy() {
}

void Enemy::Initialize(Camera* camera) {
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

	// コリジョン設定（登録するが無効化）
	auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
	if (collisionManager && object3d_ && animatedModel_) {
		Collision::AABB enemyAABB = Collision::AABBExtractor::ExtractFromAnimatedModel(animatedModel_.get());
		collisionManager->RegisterObject(object3d_.get(), enemyAABB, false, "Enemy");  // falseで無効化
	}

	// マテリアルの確認と設定 - Playerと同じロジック
	if (animatedModel_) {
		const MaterialData& material = animatedModel_->GetMaterial();
		char debugMsg[512];
		sprintf_s(debugMsg, "Enemy Model Material:\n"
			"  isPBR: %s\n"
			"  BaseColor: R=%.3f, G=%.3f, B=%.3f, A=%.3f\n"
			"  Metallic=%.3f, Roughness=%.3f\n"
			"  Texture: %s\n",
			material.isPBR ? "true" : "false",
			material.baseColorFactor.x, material.baseColorFactor.y,
			material.baseColorFactor.z, material.baseColorFactor.w,
			material.metallicFactor, material.roughnessFactor,
			material.textureFilePath.empty() ? "None" : material.textureFilePath.c_str());
		OutputDebugStringA(debugMsg);

		// PBRマテリアルでない場合、強制的にPBRを有効化
		if (!material.isPBR) {
			OutputDebugStringA("Enemy: Model is not PBR, forcing PBR settings...\n");
			MaterialData& mutableMaterial = const_cast<MaterialData&>(animatedModel_->GetMaterial());
			mutableMaterial.isPBR = true;
			mutableMaterial.baseColorFactor = { 0.8f, 0.8f, 0.8f, 1.0f };
			mutableMaterial.metallicFactor = 0.0f;
			mutableMaterial.roughnessFactor = 0.8f;
			mutableMaterial.emissiveFactor = { 0.0f, 0.0f, 0.0f };
			mutableMaterial.alphaMode = "OPAQUE";
			mutableMaterial.doubleSided = false;

			OutputDebugStringA("Enemy: Applied PBR material settings\n");
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

	OutputDebugStringA("Enemy: Initialization complete with Walk, Run, and Scream animations\n");
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
				pathUpdateTimer_ = PATH_UPDATE_INTERVAL;
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

void Enemy::DrawUI() {
	ImGui::Begin("Enemy Settings");

	// NavMesh状態表示
	ImGui::Text("NavMesh Valid: %s", (navMesh_ && navMesh_->IsValid()) ? "Yes" : "No");
	ImGui::Text("Is Chasing: %s", isChasing_ ? "Yes" : "No");

	// パス情報表示
	if (navMesh_ && navMesh_->IsValid()) {
		ImGui::Separator();
		ImGui::Text("Path Info");
		ImGui::Text("Path Waypoints: %d", static_cast<int>(currentPath_.size()));
		ImGui::Text("Current Waypoint: %d", currentWaypointIndex_);

		// 次のウェイポイントの情報を表示
		if (!currentPath_.empty() && currentWaypointIndex_ < static_cast<int>(currentPath_.size())) {
			const Vector3& nextWaypoint = currentPath_[currentWaypointIndex_];
			ImGui::Text("Next Waypoint: (%.1f, %.1f, %.1f)",
				nextWaypoint.x, nextWaypoint.y, nextWaypoint.z);

			// 現在の向きと次のウェイポイント方向を表示
			Vector3 toWaypoint = {
				nextWaypoint.x - position_.x,
				0.0f,
				nextWaypoint.z - position_.z
			};
			float targetRotation = std::atan2(toWaypoint.x, toWaypoint.z);
			ImGui::Text("Current Rotation: %.2f deg", currentRotationY_ * 180.0f / 3.14159f);
			ImGui::Text("Target Rotation: %.2f deg", targetRotation * 180.0f / 3.14159f);

			// 角検知情報
			ImGui::Text("At Corner: %s", isAtCorner_ ? "YES" : "No");
			if (isAtCorner_) {
				ImGui::Text("Corner Slowdown: %.1f%%", cornerSlowdownFactor_ * 100.0f);
			}
		}
	} else {
		ImGui::Text("NavMesh Mode: Direct Chase (No NavMesh)");
	}

	// アニメーションの有効/無効トグル
	if (ImGui::Checkbox("Animation Enabled", &animationEnabled_)) {
		if (animationEnabled_) {
			PlayAnimation();
		} else {
			PauseAnimation();
		}
	}

	// アニメーション選択
	ImGui::Separator();
	ImGui::Text("Animation Selection");

	if (ImGui::Button("Walk", ImVec2(100, 0))) {
		ChangeAnimation("Walk");
		currentAnimationIndex_ = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button("Run", ImVec2(100, 0))) {
		ChangeAnimation("Run");
		currentAnimationIndex_ = 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("Scream", ImVec2(100, 0))) {
		ChangeAnimation("Scream");
		currentAnimationIndex_ = 2;
	}

	// AI設定
	ImGui::Separator();
	ImGui::Text("AI Settings");
	ImGui::DragFloat("Detection Range", &detectionRange_, 1.0f, 0.0f, 100.0f);
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Avoidance Radius", &avoidanceRadius_, 0.1f, 0.0f, 10.0f);

	// プレイヤー検知デバッグ情報
	if (player_) {
		Vector3 playerPos = player_->GetPosition();
		Vector3 toPlayer = {
			playerPos.x - position_.x,
			0.0f,
			playerPos.z - position_.z
		};
		float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

		ImGui::Text("Player Detected: %s", player_ ? "Yes" : "No");
		ImGui::Text("Distance to Player: %.2f", distanceToPlayer);
		ImGui::Text("Is Chasing: %s", isChasing_ ? "Yes" : "No");
		ImGui::Text("Player Pos: (%.2f, %.2f, %.2f)", playerPos.x, playerPos.y, playerPos.z);
	} else {
		ImGui::Text("Player: Not Set");
	}

	// 位置コントロール
	ImGui::Separator();
	ImGui::Text("Transform");
	float pos[3] = {position_.x, position_.y, position_.z};
	if (ImGui::DragFloat3("Position", pos, 0.1f)) {
		SetPosition({pos[0], pos[1], pos[2]});
	}

	// 回転コントロール
	float rotationDeg = currentRotationY_ * 180.0f / std::numbers::pi_v<float>;
	if (ImGui::DragFloat("Rotation Y (deg)", &rotationDeg, 1.0f)) {
		currentRotationY_ = rotationDeg * std::numbers::pi_v<float> / 180.0f;
	}

	// アニメーション情報表示
	if (animatedModel_) {
		ImGui::Separator();
		ImGui::Text("Animation Info");
		std::string currentAnim = GetCurrentAnimationName();
		ImGui::Text("Current: %s", currentAnim.c_str());
		ImGui::Text("Paused: %s", animationPaused_ ? "Yes" : "No");
		ImGui::Text("Enabled: %s", animationEnabled_ ? "Yes" : "No");
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
		// TransitionToAnimationを使用してスムーズな補間を適用
		animatedModel_->TransitionToAnimation(animationName, BLEND_DURATION);
		isBlending_ = true;
		blendTimer_ = 0.0f;

		char debugMsg[256];
		sprintf_s(debugMsg, "Enemy: Transitioning to animation %s with blend duration %.2fs\n",
			animationName.c_str(), BLEND_DURATION);
		OutputDebugStringA(debugMsg);
	}
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
	auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
	if (!collisionManager || !object3d_) return;

	auto enemyColObj = collisionManager->FindCollisionObject(object3d_.get());
	if (!enemyColObj || !enemyColObj->IsEnabled()) return;

	const int maxIterations = 5;  // 反復回数を増やして確実に押し出す

	for (int iteration = 0; iteration < maxIterations; ++iteration) {
		bool hadCollision = false;
		const Collision::AABB& enemyAABB = enemyColObj->GetWorldAABB();

		for (const auto& colObj : collisionManager->GetCollisionObjects()) {
			if (colObj.get() == enemyColObj.get()) continue;
			if (!colObj->IsEnabled()) continue;

			// プレイヤーと壁との衝突は無視
			if (colObj->GetName() == "Player") continue;
			if (colObj->GetName() == "wall") continue;

			const Collision::AABB& otherAABB = colObj->GetWorldAABB();

			if (Collision::CheckAABBCollision(enemyAABB, otherAABB)) {
				hadCollision = true;

				// 押し出しベクトルを計算
				Vector3 overlapMin = {
					(std::max)(enemyAABB.min.x, otherAABB.min.x),
					(std::max)(enemyAABB.min.y, otherAABB.min.y),
					(std::max)(enemyAABB.min.z, otherAABB.min.z)
				};
				Vector3 overlapMax = {
					(std::min)(enemyAABB.max.x, otherAABB.max.x),
					(std::min)(enemyAABB.max.y, otherAABB.max.y),
					(std::min)(enemyAABB.max.z, otherAABB.max.z)
				};

				Vector3 overlap = {
					overlapMax.x - overlapMin.x,
					overlapMax.y - overlapMin.y,
					overlapMax.z - overlapMin.z
				};

				// 最小の押し出し方向を選択（Y軸は無視）
				Vector3 pushOut = {0.0f, 0.0f, 0.0f};
				const float PUSHOUT_MARGIN = 0.1f;  // 余裕を大きく
				if (overlap.x < overlap.z) {
					if (enemyAABB.GetCenter().x < otherAABB.GetCenter().x) {
						pushOut.x = -(overlap.x + PUSHOUT_MARGIN);
					} else {
						pushOut.x = overlap.x + PUSHOUT_MARGIN;
					}
				} else {
					if (enemyAABB.GetCenter().z < otherAABB.GetCenter().z) {
						pushOut.z = -(overlap.z + PUSHOUT_MARGIN);
					} else {
						pushOut.z = overlap.z + PUSHOUT_MARGIN;
					}
				}

				// 位置を補正
				position_.x += pushOut.x;
				position_.z += pushOut.z;

				object3d_->SetPosition(position_);
				object3d_->Update();

				enemyColObj->Update();
				break;
			}
		}

		if (!hadCollision) break;
	}
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

		char msg[256];
		sprintf_s(msg, "Enemy: Path updated with %d waypoints\n", path.GetWaypointCount());
		OutputDebugStringA(msg);
	} else {
		currentPath_.clear();
		currentWaypointIndex_ = 0;
	}
}

// パスに沿って移動（先読みで滑らかに）
void Enemy::FollowPath() {
	if (currentPath_.empty() || currentWaypointIndex_ >= static_cast<int>(currentPath_.size())) {
		return;
	}

	const Vector3& targetWaypoint = currentPath_[currentWaypointIndex_];

	// 目標ウェイポイントへのベクトル
	Vector3 toWaypoint = {
		targetWaypoint.x - position_.x,
		0.0f,  // Y軸は無視
		targetWaypoint.z - position_.z
	};

	float distanceToWaypoint = std::sqrt(toWaypoint.x * toWaypoint.x + toWaypoint.z * toWaypoint.z);

	// ウェイポイントに到達したら次へ（判定を緩くして曲がり角でスムーズに）
	const float WAYPOINT_REACH_THRESHOLD = 3.0f;  // さらに緩く
	if (distanceToWaypoint < WAYPOINT_REACH_THRESHOLD) {
		currentWaypointIndex_++;
		if (currentWaypointIndex_ >= static_cast<int>(currentPath_.size())) {
			// パスの終端に到達
			currentPath_.clear();
			currentWaypointIndex_ = 0;
		}
		return;
	}

	// 先読み：次のウェイポイントがある場合、そちらにも少し引き寄せられる
	Vector3 targetDirection = toWaypoint;
	isAtCorner_ = false;
	cornerSlowdownFactor_ = 1.0f;

	if (currentWaypointIndex_ + 1 < static_cast<int>(currentPath_.size())) {
		const Vector3& nextWaypoint = currentPath_[currentWaypointIndex_ + 1];
		Vector3 toNextWaypoint = {
			nextWaypoint.x - position_.x,
			0.0f,
			nextWaypoint.z - position_.z
		};

		float distToNext = std::sqrt(toNextWaypoint.x * toNextWaypoint.x + toNextWaypoint.z * toNextWaypoint.z);
		if (distToNext > 0.001f) {
			toNextWaypoint.x /= distToNext;
			toNextWaypoint.z /= distToNext;

			// 正規化された方向ベクトル
			float normalizedToWaypointX = toWaypoint.x / distanceToWaypoint;
			float normalizedToWaypointZ = toWaypoint.z / distanceToWaypoint;

			// 角度を計算（内積）
			float dotProduct = normalizedToWaypointX * toNextWaypoint.x + normalizedToWaypointZ * toNextWaypoint.z;
			float angle = std::acos(std::clamp(dotProduct, -1.0f, 1.0f));

			// 角度が大きい（急カーブ）場合は減速
			const float SHARP_TURN_THRESHOLD = 1.0f; // 約57度
			if (angle > SHARP_TURN_THRESHOLD) {
				isAtCorner_ = true;
				// 角度が急なほど減速（0.3倍～1.0倍）
				cornerSlowdownFactor_ = 0.3f + (1.0f - angle / 3.14159f) * 0.7f;
			}

			// 現在のウェイポイントに近いほど次のウェイポイントの影響を強くする
			float blendFactor = 1.0f - (distanceToWaypoint / WAYPOINT_REACH_THRESHOLD);
			blendFactor = std::clamp(blendFactor, 0.0f, 0.6f);  // 最大60%の影響

			targetDirection.x = normalizedToWaypointX * (1.0f - blendFactor) + toNextWaypoint.x * blendFactor;
			targetDirection.z = normalizedToWaypointZ * (1.0f - blendFactor) + toNextWaypoint.z * blendFactor;
		}
	}

	// 正規化
	float targetLength = std::sqrt(targetDirection.x * targetDirection.x + targetDirection.z * targetDirection.z);
	if (targetLength > 0.001f) {
		targetDirection.x /= targetLength;
		targetDirection.z /= targetLength;
	}

	// 目標回転角を計算
	targetRotationY_ = std::atan2(targetDirection.x, targetDirection.z);

	// 回転の補間（滑らかに回転）
	const float ROTATION_LERP_FACTOR = 0.15f;  // 回転の滑らかさ（0.0～1.0）

	// 角度差を-π～πの範囲に正規化
	float angleDiff = targetRotationY_ - currentRotationY_;
	while (angleDiff > 3.14159f) angleDiff -= 2.0f * 3.14159f;
	while (angleDiff < -3.14159f) angleDiff += 2.0f * 3.14159f;

	// 補間
	currentRotationY_ += angleDiff * ROTATION_LERP_FACTOR;

	// 速度の補間（滑らかに加減速）
	const float SPEED_LERP_FACTOR = 0.1f;  // 加減速の滑らかさ（0.0～1.0）
	float targetSpeed = moveSpeed_ * cornerSlowdownFactor_;
	currentSpeed_ += (targetSpeed - currentSpeed_) * SPEED_LERP_FACTOR;

	// 移動
	Vector3 newPosition = position_;
	newPosition.x += targetDirection.x * currentSpeed_;
	newPosition.z += targetDirection.z * currentSpeed_;

	// NavMesh上の有効な位置に補正
	if (navMesh_ && navMesh_->IsValid()) {
		float startPos[3] = {newPosition.x, newPosition.y, newPosition.z};
		float extents[3] = {2.0f, 4.0f, 2.0f};  // 探索範囲

		dtNavMeshQuery* query = navMesh_->GetBuilder()->GetNavMesh() ?
			dtAllocNavMeshQuery() : nullptr;

		if (query && navMesh_->GetBuilder()->GetNavMesh()) {
			query->init(navMesh_->GetBuilder()->GetNavMesh(), 2048);

			dtQueryFilter filter;
			filter.setIncludeFlags(0xffff);
			filter.setExcludeFlags(0);

			dtPolyRef nearestPoly = 0;
			float nearestPoint[3];

			// 最も近いNavMesh上の点を探す
			dtStatus status = query->findNearestPoly(startPos, extents, &filter, &nearestPoly, nearestPoint);

			if (dtStatusSucceed(status) && nearestPoly != 0) {
				// NavMesh上の有効な位置に補正
				newPosition.x = nearestPoint[0];
				newPosition.y = nearestPoint[1];
				newPosition.z = nearestPoint[2];
			}

			dtFreeNavMeshQuery(query);
		}
	}

	position_ = newPosition;
}
