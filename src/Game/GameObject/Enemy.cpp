#include "Enemy.h"
#include "Player.h"
#include "imgui.h"
#include <numbers>
#include <cmath>
#include "Collision/AABBCollision.h"

Enemy::Enemy()
	: position_({0.0f, 0.0f, 0.0f})
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
	, avoidanceRadius_(5.0f)
	, alternativeTimer_(0.0f)
	, navMeshSystem_(nullptr)
	, useNavMesh_(false) {

	// EnemyAIの初期化
	enemyAI_ = std::make_unique<EnemyAI>();
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
	float deltaTime = 1.0f / 60.0f;

	// NavMeshシステムを使用する場合
	if (useNavMesh_ && navMeshSystem_ && navMeshSystem_->IsValid() && enemyAI_) {
		// EnemyAIの位置を同期
		enemyAI_->SetPosition(position_);

		// プレイヤー位置を取得
		Vector3 playerPos = player_ ? player_->GetPosition() : Vector3{0.0f, 0.0f, 0.0f};

		// AI更新
		enemyAI_->Update(deltaTime, playerPos);

		// AI状態に応じてアニメーション変更
		EnemyState state = enemyAI_->GetState();
		if (state == EnemyState::Chase || state == EnemyState::Search) {
			if (!isChasing_) {
				ChangeAnimation("Run");
				isChasing_ = true;
			}
		} else {
			if (isChasing_) {
				ChangeAnimation("Walk");
				isChasing_ = false;
			}
		}

		// AIからの位置と回転を取得
		position_ = enemyAI_->GetPosition();
		currentRotationY_ = enemyAI_->GetRotationY();
	}
	// 従来のシンプルな追跡システム
	else {
		// 代替経路タイマーの更新
		if (alternativeTimer_ > 0.0f) {
			alternativeTimer_ -= deltaTime;
		}

		// プレイヤー検知と追跡
		if (player_) {
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

			// プレイヤーに向かって移動
			if (distanceToPlayer > 1.0f) {
				// 正規化
				float invLength = 1.0f / distanceToPlayer;
				toPlayer.x *= invLength;
				toPlayer.z *= invLength;

				// 前方をチェック（プレイヤー方向に壁があるか）
				Vector3 checkPos = {
					position_.x + toPlayer.x * 3.0f,
					position_.y,
					position_.z + toPlayer.z * 3.0f
				};

				bool forwardBlocked = CheckWallAt(checkPos);

				Vector3 moveDir = toPlayer;

				if (forwardBlocked) {
					// 前方がブロックされている場合、複数方向をチェック
					// 右、左、右斜め前、左斜め前、右後ろ、左後ろの順に試す
					Vector3 rightDir = {toPlayer.z, 0.0f, -toPlayer.x};
					Vector3 leftDir = {-toPlayer.z, 0.0f, toPlayer.x};

					// 正規化
					float rightLen = std::sqrt(rightDir.x * rightDir.x + rightDir.z * rightDir.z);
					float leftLen = std::sqrt(leftDir.x * leftDir.x + leftDir.z * leftDir.z);
					if (rightLen > 0.001f) { rightDir.x /= rightLen; rightDir.z /= rightLen; }
					if (leftLen > 0.001f) { leftDir.x /= leftLen; leftDir.z /= leftLen; }

					// 斜め方向も試す
					Vector3 rightForward = {
						toPlayer.x * 0.5f + rightDir.x * 0.5f,
						0.0f,
						toPlayer.z * 0.5f + rightDir.z * 0.5f
					};
					Vector3 leftForward = {
						toPlayer.x * 0.5f + leftDir.x * 0.5f,
						0.0f,
						toPlayer.z * 0.5f + leftDir.z * 0.5f
					};

					// 正規化
					float rfLen = std::sqrt(rightForward.x * rightForward.x + rightForward.z * rightForward.z);
					float lfLen = std::sqrt(leftForward.x * leftForward.x + leftForward.z * leftForward.z);
					if (rfLen > 0.001f) { rightForward.x /= rfLen; rightForward.z /= rfLen; }
					if (lfLen > 0.001f) { leftForward.x /= lfLen; leftForward.z /= lfLen; }

					// 各方向をチェック
					struct DirectionTest {
						Vector3 direction;
						float priority;
						bool blocked;
					};

					DirectionTest directions[] = {
						{rightForward, 0.9f, CheckWallAt({position_.x + rightForward.x * 2.5f, position_.y, position_.z + rightForward.z * 2.5f})},
						{leftForward, 0.9f, CheckWallAt({position_.x + leftForward.x * 2.5f, position_.y, position_.z + leftForward.z * 2.5f})},
						{rightDir, 0.7f, CheckWallAt({position_.x + rightDir.x * 2.5f, position_.y, position_.z + rightDir.z * 2.5f})},
						{leftDir, 0.7f, CheckWallAt({position_.x + leftDir.x * 2.5f, position_.y, position_.z + leftDir.z * 2.5f})}
					};

					// ブロックされていない方向で最も優先度の高いものを選択
					bool foundDirection = false;
					float bestPriority = -1.0f;

					for (const auto& dir : directions) {
						if (!dir.blocked && dir.priority > bestPriority) {
							moveDir = dir.direction;
							bestPriority = dir.priority;
							foundDirection = true;
						}
					}

					// どの方向もブロックされている場合は、プレイヤー方向に押し続ける
					// （衝突応答で壁から離れる）
				}

				// 移動
				position_.x += moveDir.x * moveSpeed_;
				position_.z += moveDir.z * moveSpeed_;

				// 移動方向を向く
				currentRotationY_ = std::atan2(moveDir.x, moveDir.z);
			}
		} else {
			if (isChasing_) {
				// 追跡終了：Walkアニメーションに戻す
				ChangeAnimation("Walk");
				isChasing_ = false;
			}
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

void Enemy::SetNavMeshSystem(NavMeshSystem* navMeshSystem) {
	navMeshSystem_ = navMeshSystem;

	if (navMeshSystem_ && navMeshSystem_->IsValid()) {
		useNavMesh_ = true;

		// EnemyAIを初期化
		if (enemyAI_) {
			enemyAI_->Initialize(navMeshSystem_);
			enemyAI_->SetPosition(position_);
			enemyAI_->SetDetectionRange(detectionRange_);
			enemyAI_->SetMoveSpeed(moveSpeed_ * 10.0f); // スケール調整

			char debugMsg[256];
			sprintf_s(debugMsg, "Enemy: NavMesh system initialized\n");
			OutputDebugStringA(debugMsg);
		}
	} else {
		useNavMesh_ = false;
	}
}

void Enemy::DrawUI() {
	ImGui::Begin("Enemy Settings");

	// NavMesh使用切り替え
	if (ImGui::Checkbox("Use NavMesh", &useNavMesh_)) {
		if (useNavMesh_ && enemyAI_ && navMeshSystem_) {
			enemyAI_->Initialize(navMeshSystem_);
			enemyAI_->SetPosition(position_);
		}
	}

	ImGui::SameLine();
	ImGui::Text("NavMesh Valid: %s", (navMeshSystem_ && navMeshSystem_->IsValid()) ? "Yes" : "No");

	// AI状態表示
	if (useNavMesh_ && enemyAI_) {
		ImGui::Separator();
		ImGui::Text("AI State");

		const char* stateNames[] = {"Idle", "Patrol", "Chase", "Attack", "Search"};
		int stateIndex = static_cast<int>(enemyAI_->GetState());
		ImGui::Text("Current State: %s", stateNames[stateIndex]);

		const auto& path = enemyAI_->GetCurrentPath();
		ImGui::Text("Path Waypoints: %d", static_cast<int>(path.size()));
		ImGui::Text("Current Waypoint: %d", enemyAI_->GetCurrentWaypointIndex());
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
