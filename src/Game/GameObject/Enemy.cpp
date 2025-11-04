#include "Enemy.h"
#include "Player.h"
#include "NavMesh/NavMesh.h"
#include "NavMesh/NavMeshBuilder.h"
#include "NavMesh/NavMeshHelper.h"
#include "NavMesh/EnemyAI.h"
#include "NavMesh/NavMeshSystem.h"
#include <cmath>
#include <random>
#include <fstream>
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
	animatedModel_->LoadFromFile("Resources/Models/Enemy/Enemy_Walk", "Enemy_Walk.gltf");

	// Playerと同じパターン: 読み込んだGLTFのアニメーションを取得して登録
	Animation walkAnim = animatedModel_->GetAnimationPlayer().GetAnimation();
	animatedModel_->AddAnimation("Walk", walkAnim);

	// Runアニメーションも読み込む
	std::unique_ptr<AnimatedModel> runModel = engine->CreateAnim();
	runModel->LoadFromFile("Resources/Models/Enemy/Enemy_Run", "Enemy_Run.gltf");
	Animation runAnim = runModel->GetAnimationPlayer().GetAnimation();
	animatedModel_->AddAnimation("Run", runAnim);

	// Playerと同じ: アニメーションを変更して再生
	animatedModel_->ChangeAnimation("Walk");
	animatedModel_->PlayAnimation();

	// Object3Dの作成 - Playerと全く同じ順序
	object3d_ = engine->CreateObj3();
	object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
	object3d_->SetAnimatedModel(animatedModel_.get());
	object3d_->SetPosition(position_);
	object3d_->SetScale(Vector3{0.05f, 0.05f, 0.05f });
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
			mutableMaterial.baseColorFactor = { 0.5f, 0.5f, 0.5f, 1.0f };  // より暗く
			mutableMaterial.metallicFactor = 0.0f;
			mutableMaterial.roughnessFactor = 0.8f;
			mutableMaterial.emissiveFactor = { 0.0f, 0.0f, 0.0f };  // 自己発光なし
			mutableMaterial.alphaMode = "OPAQUE";
			mutableMaterial.doubleSided = false;
			object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
		}
		else {
			// PBRマテリアルでもエミッシブを強制的にオフにして、ライティングのみに依存
			MaterialData& mutableMaterial = const_cast<MaterialData&>(animatedModel_->GetMaterial());
			mutableMaterial.emissiveFactor = { 0.0f, 0.0f, 0.0f };  // 自己発光を無効化
			object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
		}
	}

	// 3D空間オーディオの初期化 (左足と右足で別々のファイル)
	footstepSource1_ = std::make_unique<SpatialAudioSource>();
	footstepSource1_->Initialize("Resources/Audio/Enemy_feet.mp3", position_);
	footstepSource1_->SetVolume(0.8f);
	footstepSource1_->SetMaxDistance(30.0f);
	footstepSource1_->SetMinDistance(1.0f);

	footstepSource2_ = std::make_unique<SpatialAudioSource>();
	footstepSource2_->Initialize("Resources/Audio/Enemy_feet2.mp3", position_);
	footstepSource2_->SetVolume(0.8f);
	footstepSource2_->SetMaxDistance(30.0f);
	footstepSource2_->SetMinDistance(1.0f);

	// 検知サウンドの初期化
	detectionSound_ = std::make_unique<SpatialAudioSource>();
	detectionSound_->Initialize("Resources/Audio/enemysound.mp3", position_);
	detectionSound_->SetVolume(2.2f);  // 大き目の音量
	detectionSound_->SetMaxDistance(40.0f);
	detectionSound_->SetMinDistance(1.0f);

	// 足のボーンデバッグ用LineRenderer初期化
	footDebugLineRenderer_ = std::make_unique<LineRenderer>();
	footDebugLineRenderer_->Initialize(engine->GetDXCom(), camera);
}

void Enemy::Update() {
	const float deltaTime = 1.0f / 60.0f; // 60 FPS想定

#ifdef _DEBUG
	// デバッグ用：移動停止フラグが有効な場合は移動処理をスキップ（オーディオは継続）
	if (!debugStopMovement_) {
#endif

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
				if (GetCurrentAnimationName() != "Run") {
					ChangeAnimation("Run");
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

#ifdef _DEBUG
	}  // debugStopMovement_のif文の終わり
#endif

	// 重力処理
	// 地面より上にいる場合、重力を適用
	if (position_.y > GROUND_HEIGHT + GROUND_CHECK_OFFSET) {
		velocityY_ += GRAVITY * deltaTime;
		isGrounded_ = false;
	}
	else {
		// 地面に着地
		if (velocityY_ < 0.0f) {
			velocityY_ = 0.0f;
			position_.y = GROUND_HEIGHT;
			isGrounded_ = true;
		}
	}

	// Y座標を更新
	position_.y += velocityY_;

	// 地面より下に行かないように制限
	if (position_.y < GROUND_HEIGHT) {
		position_.y = GROUND_HEIGHT;
		velocityY_ = 0.0f;
		isGrounded_ = true;
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
		// 手動アニメーション制御が有効な場合
		if (debugManualAnimationControl_) {
			// 手動で時間を設定
			animatedModel_->GetAnimationPlayer().SetTime(debugManualAnimationTime_);
		}
		// 通常のアニメーション更新
		else if (!animationPaused_) {
			// デバッグ速度を適用
			animatedModel_->Update(deltaTime * debugAnimationSpeed_);
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

	// スケルトンから左右の足のボーン位置を取得
	const Skeleton& skeleton = animatedModel_->GetSkeleton();

	// 左足と右足のつま先ボーンを検索（より正確な接地検出のため）
	auto leftFootIt = skeleton.jointMap.find("mixamorig:LeftToeBase");
	auto rightFootIt = skeleton.jointMap.find("mixamorig:RightToeBase");

	// つま先ボーンが見つからない場合は足首ボーンを使用
	if (leftFootIt == skeleton.jointMap.end()) {
		leftFootIt = skeleton.jointMap.find("mixamorig:LeftFoot");
	}
	if (rightFootIt == skeleton.jointMap.end()) {
		rightFootIt = skeleton.jointMap.find("mixamorig:RightFoot");
	}

	if (leftFootIt == skeleton.jointMap.end() || rightFootIt == skeleton.jointMap.end()) {
		return;  // ボーンが見つからない場合は処理しない
	}

	// 左足と右足のジョイントを取得
	const Joint& leftFootJoint = skeleton.joints[leftFootIt->second];
	const Joint& rightFootJoint = skeleton.joints[rightFootIt->second];

	// スケルトン空間での足の位置（DrawFootBoneDebugと同じ計算）
	Vector3 leftFootSkeletonPos = {
		leftFootJoint.skeletonSpaceMatrix.m[3][0],
		leftFootJoint.skeletonSpaceMatrix.m[3][1],
		leftFootJoint.skeletonSpaceMatrix.m[3][2]
	};

	Vector3 rightFootSkeletonPos = {
		rightFootJoint.skeletonSpaceMatrix.m[3][0],
		rightFootJoint.skeletonSpaceMatrix.m[3][1],
		rightFootJoint.skeletonSpaceMatrix.m[3][2]
	};

	// モデルのスケール（0.05f）を考慮してワールド座標に変換
	const float modelScale = 0.05f;
	float worldLeftFootY = position_.y + leftFootSkeletonPos.y * modelScale;
	float worldRightFootY = position_.y + rightFootSkeletonPos.y * modelScale;

	// リスナーの位置と向きを取得
	Vector3 listenerPos = audioListener_->GetPosition();
	Vector3 listenerForward = audioListener_->GetForward();

	// 現在時刻を取得（秒）
	static float totalTime = 0.0f;
	const float deltaTime = 1.0f / 60.0f;
	totalTime += deltaTime;

	// 左足の接地判定
	bool leftFootIsAboveGround = worldLeftFootY > GROUND_HEIGHT + FOOT_GROUND_THRESHOLD;
	bool leftFootIsHighEnough = worldLeftFootY > GROUND_HEIGHT + FOOT_LIFT_THRESHOLD;  // 十分に上がったか
	bool leftFootJustLanded = leftFootWasAboveGround_ && !leftFootIsAboveGround;

	// 右足の接地判定
	bool rightFootIsAboveGround = worldRightFootY > GROUND_HEIGHT + FOOT_GROUND_THRESHOLD;
	bool rightFootIsHighEnough = worldRightFootY > GROUND_HEIGHT + FOOT_LIFT_THRESHOLD;  // 十分に上がったか
	bool rightFootJustLanded = rightFootWasAboveGround_ && !rightFootIsAboveGround;

#ifdef _DEBUG
	// debug.txtに足の状態を出力
	static int frameCounter = 0;
	if (frameCounter % 10 == 0) {  // 10フレームに1回出力（負荷軽減）
		std::ofstream debugFile("debug.txt", std::ios::app);
		if (debugFile.is_open()) {
			debugFile << "Frame " << frameCounter
			          << " | Left Y: " << worldLeftFootY << (leftFootIsAboveGround ? " [RED]" : " [GREEN]")
			          << " | Right Y: " << worldRightFootY << (rightFootIsAboveGround ? " [RED]" : " [GREEN]")
			          << " | Diff: " << std::abs(worldLeftFootY - worldRightFootY) << "\n";
			debugFile.close();
		}
	}
	frameCounter++;
#endif

	// 左足の着地処理（交互検出とクールダウン）
	if (leftFootJustLanded) {
		float timeSinceLastLand = totalTime - lastLeftFootLandTime_;
		float timeSinceAnyLand = std::min(totalTime - lastLeftFootLandTime_, totalTime - lastRightFootLandTime_);

		// 条件: (最後の着地が右足 OR 初回) AND クールダウン経過
		bool canPlayLeft = (lastFootLanded_ != LastFootLanded::Left) && (timeSinceAnyLand > FOOTSTEP_COOLDOWN);

		if (canPlayLeft) {
			// 左足が地面に着地した瞬間（赤→緑）
			if (footstepSource1_) {
				footstepSource1_->SetPosition(position_);
				footstepSource1_->Update(listenerPos, listenerForward);
				footstepSource1_->Play(false);  // ループなし

				lastLeftFootLandTime_ = totalTime;  // 着地時刻を記録
				lastFootLanded_ = LastFootLanded::Left;  // 左足が最後に着地

#ifdef _DEBUG
				// デバッグ出力とファイル出力
				OutputDebugStringA("[Enemy] Left foot landed! (RED->GREEN) Playing Enemy_feet.mp3\n");
				std::ofstream debugFile("debug.txt", std::ios::app);
				if (debugFile.is_open()) {
					debugFile << ">>> LEFT FOOT LANDED at Y=" << worldLeftFootY << " (cooldown: " << timeSinceAnyLand << "s)\n";
					debugFile.close();
				}
#endif
			}
		}
	}
	leftFootWasAboveGround_ = leftFootIsAboveGround;
	previousLeftFootY_ = worldLeftFootY;

	// 右足の着地処理（交互検出とクールダウン）
	if (rightFootJustLanded) {
		float timeSinceLastLand = totalTime - lastRightFootLandTime_;
		float timeSinceAnyLand = std::min(totalTime - lastLeftFootLandTime_, totalTime - lastRightFootLandTime_);

		// 条件: (最後の着地が左足 OR 初回) AND クールダウン経過
		bool canPlayRight = (lastFootLanded_ != LastFootLanded::Right) && (timeSinceAnyLand > FOOTSTEP_COOLDOWN);

		if (canPlayRight) {
			// 右足が地面に着地した瞬間（赤→緑）
			if (footstepSource2_) {
				footstepSource2_->SetPosition(position_);
				footstepSource2_->Update(listenerPos, listenerForward);
				footstepSource2_->Play(false);  // ループなし

				lastRightFootLandTime_ = totalTime;  // 着地時刻を記録
				lastFootLanded_ = LastFootLanded::Right;  // 右足が最後に着地

#ifdef _DEBUG
				// デバッグ出力とファイル出力
				OutputDebugStringA("[Enemy] Right foot landed! (RED->GREEN) Playing Enemy_feet2.mp3\n");
				std::ofstream debugFile("debug.txt", std::ios::app);
				if (debugFile.is_open()) {
					debugFile << ">>> RIGHT FOOT LANDED at Y=" << worldRightFootY << " (cooldown: " << timeSinceAnyLand << "s)\n";
					debugFile.close();
				}
#endif
			}
		}
	}
	rightFootWasAboveGround_ = rightFootIsAboveGround;
	previousRightFootY_ = worldRightFootY;
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
			//detectionSound_->Play(false);  // ループなし
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

	// 足のボーンデバッグ描画
	if (debugDrawFootBones_) {
		DrawFootBoneDebug();
	}
}

void Enemy::DrawDebugVision() {
	if (!player_) {
		return;
	}

	// ImGuiウィンドウで視界情報を表示
	ImGui::Begin("Enemy Vision Debug");

	// デバッグ用
#ifdef _DEBUG
	ImGui::Separator();
#endif

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

		// Runアニメーション時は速度を1.3倍に、それ以外は通常速度
		if (animationName == "Run") {
			debugAnimationSpeed_ = 1.3f;
		} else if (animationName == "Walk") {
			debugAnimationSpeed_ = 1.0f;
		}
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

// 足のボーンデバッグ描画
void Enemy::DrawFootBoneDebug() {
	if (!animatedModel_ || !footDebugLineRenderer_) {
		return;
	}

	// スケルトンから左右の足のボーン位置を取得
	const Skeleton& skeleton = animatedModel_->GetSkeleton();

	// 左足と右足のつま先ボーンを検索（より正確な接地検出のため）
	auto leftFootIt = skeleton.jointMap.find("mixamorig:LeftToeBase");
	auto rightFootIt = skeleton.jointMap.find("mixamorig:RightToeBase");

	// つま先ボーンが見つからない場合は足首ボーンを使用
	if (leftFootIt == skeleton.jointMap.end()) {
		leftFootIt = skeleton.jointMap.find("mixamorig:LeftFoot");
	}
	if (rightFootIt == skeleton.jointMap.end()) {
		rightFootIt = skeleton.jointMap.find("mixamorig:RightFoot");
	}

	if (leftFootIt == skeleton.jointMap.end() || rightFootIt == skeleton.jointMap.end()) {
		return;  // ボーンが見つからない場合は処理しない
	}

	// 左足と右足のジョイントを取得
	const Joint& leftFootJoint = skeleton.joints[leftFootIt->second];
	const Joint& rightFootJoint = skeleton.joints[rightFootIt->second];

	// スケルトン空間での足の位置（行列の4列目）
	Vector3 leftFootSkeletonPos = {
		leftFootJoint.skeletonSpaceMatrix.m[3][0],
		leftFootJoint.skeletonSpaceMatrix.m[3][1],
		leftFootJoint.skeletonSpaceMatrix.m[3][2]
	};

	Vector3 rightFootSkeletonPos = {
		rightFootJoint.skeletonSpaceMatrix.m[3][0],
		rightFootJoint.skeletonSpaceMatrix.m[3][1],
		rightFootJoint.skeletonSpaceMatrix.m[3][2]
	};

	// モデルのスケール（Object3dで設定した0.05f）を考慮してワールド座標に変換
	const float modelScale = 0.05f;
	Vector3 leftFootWorldPos = {
		position_.x + leftFootSkeletonPos.x * modelScale,
		position_.y + leftFootSkeletonPos.y * modelScale,
		position_.z + leftFootSkeletonPos.z * modelScale
	};

	Vector3 rightFootWorldPos = {
		position_.x + rightFootSkeletonPos.x * modelScale,
		position_.y + rightFootSkeletonPos.y * modelScale,
		position_.z + rightFootSkeletonPos.z * modelScale
	};

	// 接地判定
	bool leftFootGrounded = leftFootWorldPos.y <= GROUND_HEIGHT + FOOT_GROUND_THRESHOLD;
	bool rightFootGrounded = rightFootWorldPos.y <= GROUND_HEIGHT + FOOT_GROUND_THRESHOLD;

	// 色の設定（接地=緑、空中=赤）
	Vector4 leftFootColor = leftFootGrounded ? Vector4{0.0f, 1.0f, 0.0f, 1.0f} : Vector4{1.0f, 0.0f, 0.0f, 1.0f};
	Vector4 rightFootColor = rightFootGrounded ? Vector4{0.0f, 1.0f, 0.0f, 1.0f} : Vector4{1.0f, 0.0f, 0.0f, 1.0f};

	footDebugLineRenderer_->Clear();

	// グリッド描画設定
	const float gridSize = 1.0f;
	const int gridDivisions = 10;
	const float cellSize = gridSize / gridDivisions;

	// Enemyの回転を取得
	float rotationY = currentRotationY_;
	float cosRot = std::cos(rotationY);
	float sinRot = std::sin(rotationY);

	// 左足グリッド（地面に投影、Enemyの向きに回転）
	for (int i = 0; i <= gridDivisions; ++i) {
		float offset = -gridSize / 2.0f + i * cellSize;

		// X方向の線（回転適用）
		float localX1 = offset;
		float localZ1_start = -gridSize / 2.0f;
		float localZ1_end = gridSize / 2.0f;

		Vector3 start1 = {
			leftFootWorldPos.x + localX1 * cosRot - localZ1_start * sinRot,
			GROUND_HEIGHT,
			leftFootWorldPos.z + localX1 * sinRot + localZ1_start * cosRot
		};
		Vector3 end1 = {
			leftFootWorldPos.x + localX1 * cosRot - localZ1_end * sinRot,
			GROUND_HEIGHT,
			leftFootWorldPos.z + localX1 * sinRot + localZ1_end * cosRot
		};
		footDebugLineRenderer_->AddLine(start1, end1, leftFootColor);

		// Z方向の線（回転適用）
		float localX2_start = -gridSize / 2.0f;
		float localX2_end = gridSize / 2.0f;
		float localZ2 = offset;

		Vector3 start2 = {
			leftFootWorldPos.x + localX2_start * cosRot - localZ2 * sinRot,
			GROUND_HEIGHT,
			leftFootWorldPos.z + localX2_start * sinRot + localZ2 * cosRot
		};
		Vector3 end2 = {
			leftFootWorldPos.x + localX2_end * cosRot - localZ2 * sinRot,
			GROUND_HEIGHT,
			leftFootWorldPos.z + localX2_end * sinRot + localZ2 * cosRot
		};
		footDebugLineRenderer_->AddLine(start2, end2, leftFootColor);
	}

	// 右足グリッド（地面に投影、Enemyの向きに回転）
	for (int i = 0; i <= gridDivisions; ++i) {
		float offset = -gridSize / 2.0f + i * cellSize;

		// X方向の線（回転適用）
		float localX1 = offset;
		float localZ1_start = -gridSize / 2.0f;
		float localZ1_end = gridSize / 2.0f;

		Vector3 start1 = {
			rightFootWorldPos.x + localX1 * cosRot - localZ1_start * sinRot,
			GROUND_HEIGHT,
			rightFootWorldPos.z + localX1 * sinRot + localZ1_start * cosRot
		};
		Vector3 end1 = {
			rightFootWorldPos.x + localX1 * cosRot - localZ1_end * sinRot,
			GROUND_HEIGHT,
			rightFootWorldPos.z + localX1 * sinRot + localZ1_end * cosRot
		};
		footDebugLineRenderer_->AddLine(start1, end1, rightFootColor);

		// Z方向の線（回転適用）
		float localX2_start = -gridSize / 2.0f;
		float localX2_end = gridSize / 2.0f;
		float localZ2 = offset;

		Vector3 start2 = {
			rightFootWorldPos.x + localX2_start * cosRot - localZ2 * sinRot,
			GROUND_HEIGHT,
			rightFootWorldPos.z + localX2_start * sinRot + localZ2 * cosRot
		};
		Vector3 end2 = {
			rightFootWorldPos.x + localX2_end * cosRot - localZ2 * sinRot,
			GROUND_HEIGHT,
			rightFootWorldPos.z + localX2_end * sinRot + localZ2 * cosRot
		};
		footDebugLineRenderer_->AddLine(start2, end2, rightFootColor);
	}

	// 足の位置から地面までの垂直線を追加（デバッグ用）
	footDebugLineRenderer_->AddLine(leftFootWorldPos, Vector3{leftFootWorldPos.x, GROUND_HEIGHT, leftFootWorldPos.z}, leftFootColor);
	footDebugLineRenderer_->AddLine(rightFootWorldPos, Vector3{rightFootWorldPos.x, GROUND_HEIGHT, rightFootWorldPos.z}, rightFootColor);

	footDebugLineRenderer_->Render();
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
