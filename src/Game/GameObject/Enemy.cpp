#include "Enemy.h"
#include "Player.h"
#include "NavMesh.h"
#include "imgui.h"
#include <numbers>
#include <cmath>
#include "Collision/AABBCollision.h"

Enemy::Enemy()
	: position_({0.0f, 0.0f, 5.0f})
	, currentRotationY_(0.0f)
	, animationPaused_(false)
	, isBlending_(false)
	, blendTimer_(0.0f)
	, animationEnabled_(true)
	, currentAnimationIndex_(0)
	, player_(nullptr)
	, detectionRange_(20.0f)
	, moveSpeed_(0.1f)
	, isChasing_(false)
	, navMesh_(nullptr)
	, currentWaypointIndex_(0)
	, pathUpdateTimer_(0.0f)
	, lastPlayerPosition_({0.0f, 0.0f, 0.0f})
	, playerMovementThreshold_(5.0f)
	, avoidanceRadius_(5.0f)
	, alternativeTimer_(0.0f) {
}

Enemy::~Enemy() {
}

void Enemy::Initialize(Camera* camera) {
	UnoEngine* engine = UnoEngine::GetInstance();

	// Playerと全く同じ方法でアニメーションモデルを読み込む
	animatedModel_ = engine->CreateAnimatedModel();
	animatedModel_->LoadFromFile("Resources/Models/Enemy/EnemyWalk", "EnemyWalk.gltf");

	// Playerと同じパターン: 読み込んだGLTFのアニメーションを取得して登録
	Animation walkAnim = animatedModel_->GetAnimationPlayer().GetAnimation();
	animatedModel_->AddAnimation("Walk", walkAnim);

	// RunアニメーションとScreamアニメーションも読み込む
	std::unique_ptr<AnimatedModel> runModel = engine->CreateAnimatedModel();
	runModel->LoadFromFile("Resources/Models/Enemy/EnemyRun", "EnemyRun.gltf");
	Animation runAnim = runModel->GetAnimationPlayer().GetAnimation();
	animatedModel_->AddAnimation("Run", runAnim);

	std::unique_ptr<AnimatedModel> screamModel = engine->CreateAnimatedModel();
	screamModel->LoadFromFile("Resources/Models/Enemy/EnemyScream", "EnemyScream.gltf");
	Animation screamAnim = screamModel->GetAnimationPlayer().GetAnimation();
	animatedModel_->AddAnimation("Scream", screamAnim);

	// Playerと同じ: アニメーションを変更して再生
	animatedModel_->ChangeAnimation("Walk");
	animatedModel_->PlayAnimation();

	// Object3Dの作成 - Playerと全く同じ順序
	object3d_ = engine->CreateObject3D();
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

	// コリジョン設定（Playerと同じ）
	auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
	if (collisionManager && object3d_ && animatedModel_) {
		Collision::AABB enemyAABB = Collision::AABBExtractor::ExtractFromAnimatedModel(animatedModel_.get());
		collisionManager->RegisterObject(object3d_.get(), enemyAABB, true, "Enemy");
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

	OutputDebugStringA("Enemy: Initialization complete with Walk, Run, and Scream animations\n");
}

void Enemy::Update() {
	float deltaTime = 1.0f / 60.0f; // 60 FPS

	// パス更新タイマーを減らす
	pathUpdateTimer_ -= deltaTime;

	// プレイヤー検知と追跡
	if (player_ && navMesh_) {
		Vector3 playerPos = player_->GetPosition();
		Vector3 toPlayer = {
			playerPos.x - position_.x,
			0.0f,
			playerPos.z - position_.z
		};

		float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

		// デバッグ出力（1秒に1回）
		static float debugTimer = 0.0f;
		debugTimer += deltaTime;
		if (debugTimer >= 1.0f) {
			char debugMsg[256];
			sprintf_s(debugMsg, "Enemy Debug:\n  Position: (%.2f, %.2f, %.2f)\n  Player: (%.2f, %.2f, %.2f)\n  Distance: %.2f\n  Chasing: %s\n",
				position_.x, position_.y, position_.z,
				playerPos.x, playerPos.y, playerPos.z,
				distanceToPlayer,
				isChasing_ ? "YES" : "NO");
			OutputDebugStringA(debugMsg);
			debugTimer = 0.0f;
		}

		// プレイヤーが検知範囲内にいるかチェック
		if (distanceToPlayer < detectionRange_) {
			if (!isChasing_) {
				// 追跡開始：Runアニメーションに変更
				ChangeAnimation("Run");
				isChasing_ = true;
			}

			// プレイヤーが大きく移動したかチェック
			Vector3 playerMovement = {
				playerPos.x - lastPlayerPosition_.x,
				playerPos.y - lastPlayerPosition_.y,
				playerPos.z - lastPlayerPosition_.z
			};
			float playerMoved = std::sqrt(
				playerMovement.x * playerMovement.x +
				playerMovement.z * playerMovement.z
			);

			// パスの再計算が必要かチェック（0.5秒ごと、または プレイヤーが大きく移動）
			bool needsPathUpdate = (pathUpdateTimer_ <= 0.0f) ||
			                       (playerMoved > playerMovementThreshold_) ||
			                       (currentPath_.empty());

			if (needsPathUpdate) {
				// 新しいパスを計算
				currentPath_ = navMesh_->FindPath(position_, playerPos);
				currentWaypointIndex_ = 0;
				lastPlayerPosition_ = playerPos;
				pathUpdateTimer_ = 0.5f; // 0.5秒後に再計算

				if (!currentPath_.empty()) {
					char debugMsg[128];
					sprintf_s(debugMsg, "Enemy: New path calculated with %zu waypoints\n",
					         currentPath_.size());
					OutputDebugStringA(debugMsg);
				} else {
					OutputDebugStringA("Enemy: WARNING - Path calculation failed (empty path)!\n");
				}
			}

			// パスに沿って移動
			if (!currentPath_.empty() && currentWaypointIndex_ < currentPath_.size()) {
				Vector3 targetWaypoint = currentPath_[currentWaypointIndex_];
				Vector3 toWaypoint = {
					targetWaypoint.x - position_.x,
					0.0f,
					targetWaypoint.z - position_.z
				};

				float distToWaypoint = std::sqrt(
					toWaypoint.x * toWaypoint.x +
					toWaypoint.z * toWaypoint.z
				);

				// ウェイポイントに到達したら次へ
				if (distToWaypoint < 1.5f) {
					currentWaypointIndex_++;
					char debugMsg[128];
					sprintf_s(debugMsg, "Enemy: Reached waypoint %d/%zu\n",
						currentWaypointIndex_, currentPath_.size());
					OutputDebugStringA(debugMsg);
				} else {
					// ウェイポイントに向かって移動
					toWaypoint.x /= distToWaypoint;
					toWaypoint.z /= distToWaypoint;

					Vector3 oldPos = position_;
					position_.x += toWaypoint.x * moveSpeed_;
					position_.z += toWaypoint.z * moveSpeed_;

					// 移動方向を向く
					currentRotationY_ = std::atan2(toWaypoint.x, toWaypoint.z);

					// 移動を確認（最初の数フレームのみ）
					static int moveCounter = 0;
					if (moveCounter < 10) {
						char debugMsg[256];
						sprintf_s(debugMsg, "Enemy: Moving - Old:(%.2f,%.2f,%.2f) New:(%.2f,%.2f,%.2f) Speed:%.2f\n",
							oldPos.x, oldPos.y, oldPos.z,
							position_.x, position_.y, position_.z,
							moveSpeed_);
						OutputDebugStringA(debugMsg);
						moveCounter++;
					}
				}
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
		// NavMeshがない場合は従来の直進ロジック
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

void Enemy::Draw() {
	if (object3d_) {
		object3d_->Draw();
	}
}

void Enemy::DrawUI() {
	ImGui::Begin("Enemy Settings");

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
	ImGui::DragFloat("Player Move Threshold", &playerMovementThreshold_, 0.1f, 0.0f, 20.0f);

	// NavMesh情報
	ImGui::Separator();
	ImGui::Text("NavMesh");
	ImGui::Text("NavMesh: %s", navMesh_ ? "Set" : "Not Set");
	if (navMesh_) {
		ImGui::Text("Current Path: %zu waypoints", currentPath_.size());
		ImGui::Text("Current Waypoint: %d", currentWaypointIndex_);
		ImGui::Text("Path Update Timer: %.2f", pathUpdateTimer_);
	}

	// プレイヤー検知デバッグ情報
	if (player_) {
		ImGui::Separator();
		ImGui::Text("Player Tracking");
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

	const int maxIterations = 3;

	for (int iteration = 0; iteration < maxIterations; ++iteration) {
		bool hadCollision = false;
		const Collision::AABB& enemyAABB = enemyColObj->GetWorldAABB();

		for (const auto& colObj : collisionManager->GetCollisionObjects()) {
			if (colObj.get() == enemyColObj.get()) continue;
			if (!colObj->IsEnabled()) continue;

			// プレイヤーとの衝突は無視
			if (colObj->GetName() == "Player") continue;

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
				if (overlap.x < overlap.z) {
					if (enemyAABB.GetCenter().x < otherAABB.GetCenter().x) {
						pushOut.x = -(overlap.x + 0.01f); // 少し余裕を持たせる
					} else {
						pushOut.x = overlap.x + 0.01f;
					}
				} else {
					if (enemyAABB.GetCenter().z < otherAABB.GetCenter().z) {
						pushOut.z = -(overlap.z + 0.01f); // 少し余裕を持たせる
					} else {
						pushOut.z = overlap.z + 0.01f;
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
