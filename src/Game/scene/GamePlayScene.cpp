#include "GamePlayScene.h"
#include "imgui.h"
#include "UnoEngine.h"
#include "SceneManager.h"
#include "InstancedRenderer.h"
#include <cmath>
#include <algorithm>
#include <filesystem>

void GamePlayScene::Initialize() {
	if (!dxCommon_ || !srvManager_ || !camera_) {
		OutputDebugStringA("GamePlayScene::Initialize - Critical error: Required pointers are null!\n");
		return;
	}

	SceneConfigurator configurator;
	sceneData_ = configurator.LoadSceneFromJSON("Resources/Scenes/gameplay_scene.json");
	configurator.ApplySceneData(
		sceneData_, dxCommon_, srvManager_, camera_,
		player_, enemy_, sceneObjects_, skybox_, lightManager_,
		fpsCamera_, postProcess_, skyboxEnabled_,
		fisheyeStrength_, fisheyeRadius_
	);

	// ナビメッシュの初期化（UnoEngine経由）
	UnoEngine* engine = UnoEngine::GetInstance();
	NavMeshManager* navMeshManager = engine->GetNavMgr();

	if (navMeshManager) {
		navMeshManager->SetLogCallback([this](const std::string& message) {
			AddNavMeshLog(message);
			});
	}

	const std::string navMeshPath = "externals/navimap/stage.navmesh";
	engine->InitNav(navMeshPath);

	// NavMeshが読み込まれなかった場合は生成
	navMeshManager = engine->GetNavMgr();
	if (!navMeshManager->GetNavMesh() || !navMeshManager->GetNavMesh()->IsValid()) {
		AddNavMeshLog("No existing NavMesh found, auto-generating...");
		engine->GenNav(sceneObjects_, navMeshPath);
	}

	// 3D空間オーディオリスナーの初期化
	audioListener_ = std::make_unique<SpatialAudioListener>();
	if (player_) {
		audioListener_->SetPosition(player_->GetPosition());
	}

	// EnemyにNavMeshとAudioListenerを設定
	if (enemy_) {
		navMeshManager = engine->GetNavMgr();
		if (navMeshManager && navMeshManager->GetNavMesh()) {
			enemy_->SetNavMesh(navMeshManager->GetNavMesh());
			AddNavMeshLog("NavMesh set to Enemy");
		}
		if (player_) {
			enemy_->SetPlayer(player_.get());
		}
		if (audioListener_) {
			enemy_->SetAudioListener(audioListener_.get());
			AddNavMeshLog("Player and AudioListener set to Enemy");
		}
	}

	// Orbの初期化（70個）
	orbs_.clear();

	// JSONファイルからOrb位置を読み込み
	std::vector<Vector3> orbPositions;
	const std::string orbPositionFile = "Resources/Models/orb/orb_positions.json";

	if (!JsonLoader::LoadOrbPositions(orbPositionFile, orbPositions)) {
		OutputDebugStringA("ERROR: Failed to load orb positions from JSON!\n");
		return; // JSONファイルが読み込めない場合は初期化を中断
	}
	OutputDebugStringA(("Successfully loaded " + std::to_string(orbPositions.size()) + " orb positions from JSON\n").c_str());

	// 各位置にOrbを生成
	for (const auto& pos : orbPositions) {
		auto orb = std::make_unique<Orb>();
		orb->Initialize(pos, camera_);
		orbs_.push_back(std::move(orb));
	}

	OutputDebugStringA(("GamePlayScene: Initialized " + std::to_string(orbs_.size()) + " Orbs\n").c_str());

	// Orb取得音の読み込み
	AudioManager::GetInstance()->LoadMP3("orbGet", "Resources/Audio/get.mp3");
	AudioManager::GetInstance()->SetVolume("orbGet", 0.5f);

	// 暗転用スプライトの初期化（黒い四角形）
	fadeSprite_ = std::make_unique<Sprite>();
	fadeSprite_->Initialize(spriteCommon_, "Resources/textures/white1x1.png");
	fadeSprite_->SetPosition({0.0f, 0.0f});
	fadeSprite_->SetSize({1280.0f, 720.0f});  // 画面全体をカバー
	fadeSprite_->setColor({0.0f, 0.0f, 0.0f, 0.0f});  // 初期状態は透明
}


void GamePlayScene::Update() {
	UnoEngine* engine = UnoEngine::GetInstance();
	const float deltaTime = engine->GetDelta();

	// リスポーン処理の更新
	UpdateRespawn(deltaTime);

	// フェードスプライトのアルファ値を更新
	if (fadeSprite_) {
		fadeSprite_->setColor({0.0f, 0.0f, 0.0f, fadeAlpha_});
		fadeSprite_->Update();
	}

	// リスポーン処理中は通常のゲームロジックをスキップ
	if (respawnState_ != RespawnState::None) {
		return;
	}

	// ウィンドウサイズが変わったときにPostProcessのレンダーターゲットをリサイズ
	static uint32_t previousWidth = 0;
	static uint32_t previousHeight = 0;
	uint32_t currentWidth = dxCommon_->GetCurrentWindowWidth();
	uint32_t currentHeight = dxCommon_->GetCurrentWindowHeight();

	if (postProcess_ && (previousWidth != currentWidth || previousHeight != currentHeight)) {
		if (previousWidth != 0 && previousHeight != 0) {  // 初回は除外
			postProcess_->ResizeRenderTarget();
		}
		previousWidth = currentWidth;
		previousHeight = currentHeight;
	}

#ifdef _DEBUG
	// デバッグ用：Fog & Lighting調整
	ImGui::Begin("Debug: Visibility");

	// Fog設定
	ImGui::Text("Fog Settings");
	static bool enableFog = true;
	if (ImGui::Checkbox("Enable Fog", &enableFog)) {
		// 全てのObject3dのFogを切り替え
		for (auto& obj : sceneObjects_) {
			obj->SetFogEnabled(enableFog);
		}
	}

	// ライティング設定
	ImGui::Separator();
	ImGui::Text("Lighting Settings");
	if (lightManager_) {
		static float lightBoost = 1.0f;
		if (ImGui::SliderFloat("Light Intensity Boost", &lightBoost, 1.0f, 100.0f)) {
			lightManager_->SetLightBoost(lightBoost);
		}
		if (ImGui::Button("Reset Light")) {
			lightBoost = 1.0f;
			lightManager_->ResetLightBoost();
		}
	}

	ImGui::Separator();
	ImGui::SliderFloat("Fisheye Strength", &fisheyeStrength_, 0.0f, 100.0f);
	ImGui::SliderFloat("Fisheye Radius", &fisheyeRadius_, 0.1f, 3.0f);

	// カリング統計の表示
	ImGui::Separator();
	ImGui::Text("=== Culling Statistics ===");
	ImGui::Text("Total Objects: %d", cullingStats_.totalObjects);
	ImGui::Text("Visible Objects: %d", cullingStats_.visibleObjects);
	ImGui::Text("Culled Objects: %d", cullingStats_.culledObjects);
	ImGui::Text("Culling Rate: %.1f%%", cullingStats_.cullingRate);
	ImGui::Separator();
	ImGui::Text("Visible Meshes: %d", cullingStats_.visibleMeshes);
	ImGui::Text("Culled Meshes: %d", cullingStats_.culledMeshes);
	if (cullingStats_.visibleMeshes + cullingStats_.culledMeshes > 0) {
		float meshCullingRate = (float)cullingStats_.culledMeshes /
			(cullingStats_.visibleMeshes + cullingStats_.culledMeshes) * 100.0f;
		ImGui::Text("Mesh Culling Rate: %.1f%%", meshCullingRate);
	}

	// パフォーマンス向上の目安
	ImGui::Separator();
	if (cullingStats_.cullingRate > 50.0f) {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Good culling efficiency!");
	}
	else if (cullingStats_.cullingRate > 25.0f) {
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Moderate culling efficiency");
	}
	else {
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Low culling efficiency");
	}

	//Enemy設定
	ImGui::Separator();
	ImGui::Text("Enemy Settings");
	if (enemy_) {
		ImGui::Checkbox("Stop Enemy Move", &enemy_->debugStopMovement_);
		ImGui::Checkbox("Draw Foot Bones", &enemy_->debugDrawFootBones_);

		ImGui::Separator();
		ImGui::Text("Animation Control:");

		// アニメーション速度スライダー
		ImGui::SliderFloat("Animation Speed", &enemy_->debugAnimationSpeed_, 0.0f, 2.0f, "%.2fx");
		if (ImGui::Button("Reset Speed")) {
			enemy_->debugAnimationSpeed_ = 1.0f;
		}

		// 手動アニメーション制御
		ImGui::Checkbox("Manual Control", &enemy_->debugManualAnimationControl_);
		if (enemy_->debugManualAnimationControl_ && enemy_->GetModel()) {
			float maxTime = enemy_->GetModel()->GetAnimationPlayer().GetDuration();
			ImGui::SliderFloat("Animation Time", &enemy_->debugManualAnimationTime_, 0.0f, maxTime, "%.3fs");
			if (ImGui::Button("Reset Time")) {
				enemy_->debugManualAnimationTime_ = 0.0f;
			}
		}

		// 足のボーン位置デバッグ情報
		if (enemy_->debugDrawFootBones_ && enemy_->GetModel()) {
			ImGui::Separator();
			ImGui::Text("Foot Debug Info:");

			const Skeleton& skeleton = enemy_->GetModel()->GetSkeleton();

			// つま先ボーンを検索（UpdateFootstepAudioと同じロジック）
			auto leftFootIt = skeleton.jointMap.find("mixamorig:LeftToeBase");
			auto rightFootIt = skeleton.jointMap.find("mixamorig:RightToeBase");

			if (leftFootIt == skeleton.jointMap.end()) {
				leftFootIt = skeleton.jointMap.find("mixamorig:LeftFoot");
			}
			if (rightFootIt == skeleton.jointMap.end()) {
				rightFootIt = skeleton.jointMap.find("mixamorig:RightFoot");
			}

			if (leftFootIt != skeleton.jointMap.end() && rightFootIt != skeleton.jointMap.end()) {
				const Joint& leftFootJoint = skeleton.joints[leftFootIt->second];
				const Joint& rightFootJoint = skeleton.joints[rightFootIt->second];

				float leftFootY = leftFootJoint.skeletonSpaceMatrix.m[3][1];
				float rightFootY = rightFootJoint.skeletonSpaceMatrix.m[3][1];

				const float modelScale = 0.05f;
				float leftFootWorldY = enemy_->GetPosition().y + leftFootY * modelScale;
				float rightFootWorldY = enemy_->GetPosition().y + rightFootY * modelScale;

				bool leftGrounded = leftFootWorldY <= 0.0f + 0.15f;
				bool rightGrounded = rightFootWorldY <= 0.0f + 0.15f;

				ImGui::Text("Left Foot Y: %.3f %s", leftFootWorldY, leftGrounded ? "[GREEN]" : "[RED]");
				ImGui::Text("Right Foot Y: %.3f %s", rightFootWorldY, rightGrounded ? "[GREEN]" : "[RED]");
				ImGui::Text("Difference: %.3f", std::abs(leftFootWorldY - rightFootWorldY));
				ImGui::Text("Ground Level: 0.000");
				ImGui::Text("Threshold: 0.150");
			}
		}
	}

	ImGui::End();
#endif

	// 魚眼強度と範囲を適用
	if (postProcess_) {
		postProcess_->SetFisheyeStrength(fisheyeStrength_);
		postProcess_->SetFisheyeRadius(fisheyeRadius_);
	}

	player_->HandleInput(engine);
	HandleInput();

	// ジャンプスケア中は通常のカメラ更新をスキップ
	if (!jumpscareStarted_) {
		// FPSカメラモードかどうかでカメラ更新を切り替え
		if (fpsCamera_ && fpsCamera_->IsFPSMode()) {
			// FPSモード: FPSカメラ専用の更新
			fpsCamera_->UpdateCameraRotation(camera_, engine);

			// カメラシェイクを更新（プレイヤーの移動状態に基づく）
			fpsCamera_->UpdateCameraShake(player_->IsMoving(), player_->IsRunning(), deltaTime, engine);

			player_->UpdateFPSCamera(fpsCamera_.get());
			camera_->Update();
		}
		else {
			// 三人称モード: 通常のカメラシステム
			player_->UpdateCameraSystem(engine);
		}
	}

	lightManager_->Update(engine->GetDelta());

	// スポットライトをプレイヤー視点に追従させる
	if (fpsCamera_) {
		lightManager_->UpdateFlashlight(player_->GetPosition(), fpsCamera_->GetCameraRotation());
	}

	const DirectionalLight& dirLight = lightManager_->GetDirectionalLight();
	const SpotLight& spotLight = lightManager_->GetSpotLight();

	player_->SetDirectionalLight(dirLight);
	player_->SetSpotLight(spotLight);

	// AudioListenerの位置と向きを更新（プレイヤーの位置とカメラの向き）
	if (audioListener_ && player_ && camera_) {
		audioListener_->SetPosition(player_->GetPosition());
		// カメラの回転からforward vectorを計算
		Vector3 cameraRot = camera_->GetRotate();
		Vector3 forward = {
			std::sin(cameraRot.y),
			0.0f,
			std::cos(cameraRot.y)
		};
		audioListener_->SetOrientation(forward, Vector3{ 0.0f, 1.0f, 0.0f });
	}

	if (enemy_) {
		enemy_->SetDirectionalLight(const_cast<DirectionalLight*>(&dirLight));
		enemy_->SetSpotLight(const_cast<SpotLight*>(&spotLight));
		enemy_->Update(UnoEngine::GetInstance());

		// プレイヤーとの距離をチェック（ジャンプスケア判定）
		if (player_ && !isGameOver_ && !enemy_->IsJumpscaring()) {
			Vector3 playerPos = player_->GetPosition();
			Vector3 enemyPos = enemy_->GetPosition();

			// プレイヤーとEnemyの距離を計算
			float dx = enemyPos.x - playerPos.x;
			float dy = enemyPos.y - playerPos.y;
			float dz = enemyPos.z - playerPos.z;
			float distance = sqrtf(dx * dx + dy * dy + dz * dz);

			// 距離が5m未満ならジャンプスケア開始
			if (distance < GAMEOVER_DISTANCE) {
				enemy_->StartJumpscare();
				// プレイヤーを動けなくする
				player_->SetJumpscareMode(true);
				jumpscareStarted_ = true;
				OutputDebugStringA("Jumpscare started! Player movement disabled.\n");
			}
		}

		// ジャンプスケア中のカメラ制御
		if (enemy_->IsJumpscaring() && jumpscareStarted_ && camera_ && player_) {
			Vector3 playerPos = player_->GetPosition();
			Vector3 enemyHeadPos = enemy_->GetHeadPosition();  // 顔の位置を取得

			// プレイヤーから顔への方向ベクトル
			Vector3 playerToHead = {
				enemyHeadPos.x - playerPos.x,
				enemyHeadPos.y - playerPos.y,
				enemyHeadPos.z - playerPos.z
			};

			// 正規化
			float length = std::sqrt(playerToHead.x * playerToHead.x +
			                         playerToHead.y * playerToHead.y +
			                         playerToHead.z * playerToHead.z);

			if (length > 0.0f) {
				playerToHead.x /= length;
				playerToHead.y /= length;
				playerToHead.z /= length;
			}

			// 元の方向から少し左に回転（-30度）
			const float angleOffset = -30.0f * 3.14159f / 180.0f;  // ラジアンに変換
			float cosAngle = std::cos(angleOffset);
			float sinAngle = std::sin(angleOffset);

			// Y軸周りの回転（水平方向のみ）
			Vector3 adjustedDirection = {
				playerToHead.x * cosAngle - playerToHead.z * sinAngle,
				playerToHead.y,
				playerToHead.x * sinAngle + playerToHead.z * cosAngle
			};

			// カメラを顔の前に配置（調整した方向で）
			const float cameraDistance = 3.0f;  // 顔から3.0m離れた位置
			const float heightOffset = -0.4f;  // 顔より少し下（見上げる角度）
			Vector3 cameraPos = {
				enemyHeadPos.x - adjustedDirection.x * cameraDistance,
				enemyHeadPos.y + heightOffset,  // 顔より少し下から
				enemyHeadPos.z - adjustedDirection.z * cameraDistance
			};
			camera_->SetTranslate(cameraPos);

			// エネミーの顔を見るようにカメラの回転を計算
			Vector3 cameraToHead = {
				enemyHeadPos.x - cameraPos.x,
				enemyHeadPos.y - cameraPos.y,
				enemyHeadPos.z - cameraPos.z
			};

			float horizontalDist = std::sqrt(cameraToHead.x * cameraToHead.x + cameraToHead.z * cameraToHead.z);
			float rotY = std::atan2(cameraToHead.x, cameraToHead.z);
			float rotX = -std::atan2(cameraToHead.y, horizontalDist);

			camera_->SetRotate({rotX, rotY, 0.0f});
			camera_->Update();

			// スポットライトを顔全体に向けて強く照らす
			if (lightManager_) {
				SpotLight jumpscareLight;
				jumpscareLight.position = cameraPos;  // カメラと同じ位置
				jumpscareLight.direction = cameraToHead;  // 顔に向ける
				jumpscareLight.color = {1.0f, 1.0f, 1.0f, 1.0f};  // 白色
				jumpscareLight.intensity = 15.0f;  // 強い光
				jumpscareLight.innerCone = std::cos(45.0f * 3.14159f / 180.0f);  // 45度の内側角度
				jumpscareLight.outerCone = std::cos(60.0f * 3.14159f / 180.0f);  // 60度の外側角度
				jumpscareLight.attenuation = {1.0f, 0.1f, 0.01f};  // 減衰パラメータ（定数、線形、二次）

				// 一時的にスポットライトを更新
				lightManager_->SetJumpscareLight(jumpscareLight);
			}
		}

		// ジャンプスケア終了後の処理
		if (enemy_->IsJumpscareFinished() && !isGameOver_) {
			captureCount_++;
			OutputDebugStringA(("Captured! Count: " + std::to_string(captureCount_) + "/" + std::to_string(MAX_CAPTURES) + "\n").c_str());

			if (captureCount_ >= MAX_CAPTURES) {
				// 3回目はゲームオーバー - 先にジャンプスケアプロセスを起動してからゲーム終了
				isGameOver_ = true;
				OutputDebugStringA("Max captures reached, launching jumpscare process and exiting immediately...\n");

				// 自分自身を--jumpscareオプション付きで起動
				char exePath[MAX_PATH];
				GetModuleFileNameA(nullptr, exePath, MAX_PATH);

				STARTUPINFOA si = {};
				si.cb = sizeof(si);
				si.dwFlags = STARTF_USESHOWWINDOW;
				si.wShowWindow = SW_HIDE;

				PROCESS_INFORMATION pi = {};

				// --jumpscareオプションを付けて起動
				char cmdLine[MAX_PATH + 20];
				sprintf_s(cmdLine, "\"%s\" --jumpscare", exePath);

				BOOL processCreated = CreateProcessA(
					nullptr,
					cmdLine,
					nullptr,
					nullptr,
					FALSE,
					DETACHED_PROCESS,  // 完全に独立したプロセスとして起動
					nullptr,
					nullptr,
					&si,
					&pi
				);

				if (processCreated) {
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
					OutputDebugStringA("Jumpscare process launched successfully.\n");
				} else {
					OutputDebugStringA("Failed to launch jumpscare process!\n");
				}

				// ゲームを即座に強制終了
				OutputDebugStringA("Terminating game immediately...\n");
				ExitProcess(0);  // PostQuitMessageではなくExitProcessで即座に終了
			} else {
				// 3回未満ならリスポーン開始
				StartRespawn();
			}
		}

		// 追跡モード時の距離に応じたビネット効果とカメラ振動
		if (enemy_->IsChasing() && player_ && postProcess_) {
			Vector3 playerPos = player_->GetPosition();
			Vector3 enemyPos = enemy_->GetPosition();

			// プレイヤーとEnemyの距離を計算
			float dx = enemyPos.x - playerPos.x;
			float dy = enemyPos.y - playerPos.y;
			float dz = enemyPos.z - playerPos.z;
			float distance = sqrtf(dx * dx + dy * dy + dz * dz);

			// 距離に応じてビネット強度を計算（近いほど強く）
			const float MIN_DISTANCE = 3.0f;   // この距離で最大効果
			const float MAX_DISTANCE = 15.0f;  // この距離で効果なし

			float vignetteIntensity = 0.0f;
			float fearShakeIntensity = 0.0f;

			if (distance < MAX_DISTANCE) {
				// 距離を0.0～1.0の範囲に正規化（近いほど1.0）
				float normalizedDistance = 1.0f - ((distance - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE));
				normalizedDistance = (std::max)(0.0f, (std::min)(1.0f, normalizedDistance));

				// 黒いビネット効果を適用
				vignetteIntensity = normalizedDistance * 0.8f;  // 黒いビネット

				// カメラ振動の強度を設定
				fearShakeIntensity = normalizedDistance;  // 0.0～1.0

				// ポストエフェクトに設定
				static float time = 0.0f;
				time += deltaTime;
				postProcess_->SetHorrorParams(time, 0.0f, 0.0f, 0.0f, vignetteIntensity);
			}
			else {
				// 距離が遠い時はエフェクトをリセット
				static float time = 0.0f;
				time += deltaTime;
				postProcess_->SetHorrorParams(time, 0.0f, 0.0f, 0.0f, 0.0f);
				fearShakeIntensity = 0.0f;
			}

			// FPSカメラに恐怖シェイクの強度を設定（無効化）
			// if (fpsCamera_) {
			//     fpsCamera_->SetFearShakeIntensity(fearShakeIntensity);
			// }

			// ライトマネージャーに恐怖点滅の強度を設定
			if (lightManager_) {
				lightManager_->SetFearFlickerIntensity(fearShakeIntensity);
			}
		}
		else {
			// 追跡していない時はエフェクトをリセット
			if (postProcess_) {
				static float time = 0.0f;
				time += deltaTime;
				postProcess_->SetHorrorParams(time, 0.0f, 0.0f, 0.0f, 0.0f);
			}
			// if (fpsCamera_) {
			//     fpsCamera_->SetFearShakeIntensity(0.0f);
			// }
			if (lightManager_) {
				lightManager_->SetFearFlickerIntensity(0.0f);
			}
		}
	}

	// 全シーンオブジェクトを更新
	for (auto& obj : sceneObjects_) {
		obj->SetDirectionalLight(dirLight);
		obj->SetSpotLight(spotLight);
		obj->Update();
	}

	if (skyboxEnabled_ && skybox_) {
		skybox_->Update();
	}
	player_->Update(engine);

	// Orbの更新と衝突判定
	for (auto& orb : orbs_) {
		if (orb) {
			// ライト設定
			orb->SetDirectionalLight(dirLight);
			orb->SetSpotLight(spotLight);

			// 更新
			orb->Update(deltaTime);

			// プレイヤーとの衝突判定（簡易的な球体判定）
			if (player_) {
				Vector3 playerPos = player_->GetPosition();
				float playerRadius = 0.5f;  // プレイヤーの衝突半径（小さくしてより近づく必要がある）

				if (orb->CheckCollisionWithPlayer(playerPos, playerRadius)) {
					OutputDebugStringA("Orb collected!\n");

					// Orb取得音を再生
					AudioManager::GetInstance()->Play("orbGet", false);

					// 残りのOrbを数える
					int remainingOrbs = 0;
					for (const auto& o : orbs_) {
						if (o && !o->IsCollected()) {
							remainingOrbs++;
						}
					}

					char debugMsg[256];
					sprintf_s(debugMsg, "Remaining Orbs: %d / 70\n", remainingOrbs);
					OutputDebugStringA(debugMsg);

					if (remainingOrbs == 0) {
						OutputDebugStringA("All Orbs collected! Congratulations!\n");
					}
				}
			}
		}
	}

	// NavMesh更新（UnoEngine経由）
	engine->UpdateNavMesh();
}

void GamePlayScene::Draw() {
	// ポストプロセス用のレンダーターゲットに描画
	if (postProcess_) {
		postProcess_->PreDraw();
	}

	if (skyboxEnabled_ && skybox_) {
		skybox_->Draw(camera_);
	}

	spriteCommon_->CommonDraw();

	// カリング統計をリセット
	cullingStats_.totalObjects = 0;
	cullingStats_.visibleObjects = 0;
	cullingStats_.culledObjects = 0;
	cullingStats_.visibleMeshes = 0;
	cullingStats_.culledMeshes = 0;

	// 全シーンオブジェクトを描画（カリング統計付き）
	for (auto& obj : sceneObjects_) {
		int visibleMeshCount = 0;
		int culledMeshCount = 0;
		obj->Draw(camera_, &visibleMeshCount, &culledMeshCount);

		cullingStats_.totalObjects++;
		cullingStats_.visibleMeshes += visibleMeshCount;
		cullingStats_.culledMeshes += culledMeshCount;

		if (visibleMeshCount > 0) {
			cullingStats_.visibleObjects++;
		}
		else {
			cullingStats_.culledObjects++;
		}
	}

	// カリング率を計算
	if (cullingStats_.totalObjects > 0) {
		cullingStats_.cullingRate = (float)cullingStats_.culledObjects / cullingStats_.totalObjects * 100.0f;
	}

	if (!fpsCamera_ || !fpsCamera_->IsFPSMode()) {
		player_->Draw();
	}

	// Orbの描画（常に描画する）
	for (auto& orb : orbs_) {
		if (orb) {
			orb->Draw();
		}
	}

	// Enemyを描画（ジャンプスケア中も同じモデルでアニメーションが切り替わる）
	if (enemy_) {
		enemy_->Draw();
	}

	// NavMeshの視覚化（UnoEngine経由）
	UnoEngine::GetInstance()->DrawNavVis();

	// NavMeshデバッグプレビュー描画
	{
		auto* navMeshManager = UnoEngine::GetInstance()->GetNavMgr();
		if (navMeshManager) {
			navMeshManager->DrawDebugPreview();
		}
	}

	// ポストプロセスを適用して画面に描画
	if (postProcess_) {
		postProcess_->PostDraw();
	}

	// 暗転エフェクトを最前面に描画（リスポーン中）
	if (fadeAlpha_ > 0.0f && fadeSprite_) {
		spriteCommon_->CommonDraw();
		fadeSprite_->Draw();
	}

#ifdef _DEBUG
	player_->DrawUI();

	if (lightManager_) {
		lightManager_->DrawImGui();
	}

	// NavMeshデバッグウィンドウ
	if (showNavMeshDebug_) {
		ImGui::Begin("NavMesh Debug (M キーで表示切替)");

		UnoEngine* engine = UnoEngine::GetInstance();
		NavMeshManager* navMeshManager = engine->GetNavMgr();

		if (navMeshManager) {
			// NavMeshManagerのImGui描画
			bool showViz = engine->IsNavVis();
			if (ImGui::Checkbox("Show NavMesh Visualization", &showViz)) {
				if (showViz) {
					// 視覚化を有効にする場合、視覚化オブジェクトを作成
					engine->CreateNavVis();
					engine->SetNavVis(true);
					AddNavMeshLog("NavMesh visualization enabled");
				}
				else {
					// 視覚化を無効にする
					engine->SetNavVis(false);
					AddNavMeshLog("NavMesh visualization disabled");
				}
			}

			// Enemy視界の可視化
			if (enemy_) {
				bool showEnemyVision = enemy_->debugDrawVision_;
				if (ImGui::Checkbox("Show Enemy Vision", &showEnemyVision)) {
					enemy_->debugDrawVision_ = showEnemyVision;
				}
			}

			if (ImGui::CollapsingHeader("About Recast Navigation")) {
				ImGui::TextWrapped("This project uses Recast Navigation, the industry-standard NavMesh library.");
				ImGui::TextWrapped("Used in: Unreal Engine, Unity, many AAA games");
				ImGui::Separator();
				ImGui::Text("Precision depends on settings:");
				ImGui::BulletText("Lower Cell Size = Higher precision (0.1-0.2 recommended)");
				ImGui::BulletText("Smaller Agent Radius = Closer to walls (0.3-0.6)");
				ImGui::BulletText("Lower Edge Max Error = Smoother paths (0.5-1.0)");
				ImGui::Separator();
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "This is professional-grade pathfinding!");
				ImGui::TextWrapped("If precision seems low, try the High Precision Preset below.");
			}

			NavMeshBuildSettings& settings = engine->GetNavSet();

			// リアルタイムプレビュー機能
			static bool showPreview = true;  // 最初からON
			static NavMeshBuildSettings lastSettings = settings;
			bool settingsChanged = false;

			// プレビュー範囲の設定
			static float previewRadius = 30.0f;  // Enemyの周りに表示する範囲
			static bool useEnemyCenter = true;   // Enemyを中心にするか

			// 初回のみDebugPreviewを有効化
			static bool initialized = false;
			if (!initialized) {
				navMeshManager->SetDebugPreviewEnabled(true);
				initialized = true;
			}

			if (ImGui::Checkbox("Show Grid Preview (Real-time)", &showPreview)) {
				navMeshManager->SetDebugPreviewEnabled(showPreview);
			}

			if (showPreview) {
				ImGui::SameLine();
				ImGui::Checkbox("Center on Enemy", &useEnemyCenter);

				// Agent設定に基づいた表示範囲
				static bool useAgentSettings = true;
				static bool showBoundingBox = false;  // 黄色い箱は最初はOFF
				static bool showGrid = false;  // グリッドも最初はOFF
				ImGui::Checkbox("Use Agent Settings for Preview", &useAgentSettings);
				ImGui::Checkbox("Show Grid (Cyan)", &showGrid);
				ImGui::Checkbox("Show Bounding Box (Yellow)", &showBoundingBox);

				if (!useAgentSettings) {
					ImGui::SliderFloat("Preview Radius", &previewRadius, 10.0f, 100.0f);
				}

				// バウンド計算
				Vector3 center;
				if (useEnemyCenter && enemy_) {
					center = enemy_->GetPosition();
				}
				else {
					// シーン全体を表示
					center = { 0.0f, 3.0f, 15.55f };
				}

				// Agent設定に基づいた範囲計算
				float displayRadius = previewRadius;
				if (useAgentSettings) {
					// Agent Radiusの5倍程度を表示範囲とする
					displayRadius = settings.agentRadius * 5.0f;
					if (displayRadius < 10.0f) displayRadius = 10.0f;
					if (displayRadius > 50.0f) displayRadius = 50.0f;
				}

				Vector3 minBounds = {
					center.x - displayRadius,
					center.y - settings.agentHeight,
					center.z - displayRadius
				};
				Vector3 maxBounds = {
					center.x + displayRadius,
					center.y + settings.agentHeight,
					center.z + displayRadius
				};

				// 毎フレーム更新（Enemyが動いた場合も反映）
				navMeshManager->SetPreviewBounds(minBounds, maxBounds);
				navMeshManager->CreateDebugPreview(engine->GetDXCom(), engine->GetCamera(), settings, enemy_ ? enemy_->GetPosition() : center, showBoundingBox, showGrid);

				// デバッグ情報表示
				ImGui::Text("Debug Info:");
				ImGui::Text("  Preview Enabled: %s", navMeshManager->IsDebugPreviewEnabled() ? "YES" : "NO");
				ImGui::Text("  Center: (%.1f, %.1f, %.1f)", center.x, center.y, center.z);
				ImGui::Text("  Display Radius: %.1f", displayRadius);
				ImGui::Text("  Agent Radius: %.2f", settings.agentRadius);
				ImGui::Text("  Agent Height: %.2f", settings.agentHeight);
				ImGui::Text("  Cell Size: %.3f", settings.cellSize);
			}

			ImGui::Separator();

			if (ImGui::CollapsingHeader("NavMesh Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Quick Fix: Apply High Precision Preset");
				ImGui::Separator();

				if (ImGui::Button("Apply High Precision Preset")) {
					settings.cellSize = 0.15f;
					settings.cellHeight = 0.2f;
					settings.agentHeight = 2.0f;
					settings.agentRadius = 0.4f;
					settings.agentMaxClimb = 0.5f;
					settings.agentMaxSlope = 45.0f;
					settings.edgeMaxError = 0.8f;
					settings.detailSampleDist = 6.0f;
					AddNavMeshLog("Applied high precision preset");
					settingsChanged = true;
				}

				ImGui::Separator();
				if (ImGui::SliderFloat("Cell Size", &settings.cellSize, 0.05f, 1.0f)) settingsChanged = true;
				if (ImGui::SliderFloat("Cell Height", &settings.cellHeight, 0.05f, 0.5f)) settingsChanged = true;
				if (ImGui::SliderFloat("Agent Height", &settings.agentHeight, 0.5f, 5.0f)) settingsChanged = true;
				if (ImGui::SliderFloat("Agent Radius", &settings.agentRadius, 0.1f, 5.0f)) settingsChanged = true;
				if (ImGui::SliderFloat("Agent Max Climb", &settings.agentMaxClimb, 0.1f, 1.0f)) settingsChanged = true;
				if (ImGui::SliderFloat("Agent Max Slope", &settings.agentMaxSlope, 0.0f, 90.0f)) settingsChanged = true;

				ImGui::Separator();
				ImGui::Text("Corner Smoothness Settings");
				if (ImGui::SliderFloat("Edge Max Error", &settings.edgeMaxError, 0.1f, 3.0f)) settingsChanged = true;
				if (ImGui::SliderFloat("Detail Sample Dist", &settings.detailSampleDist, 1.0f, 10.0f)) settingsChanged = true;

			}

			if (ImGui::Button("Generate NavMesh")) {
				ClearNavMeshLogs();
				AddNavMeshLog("=== Manual NavMesh generation triggered ===");
				engine->GenNav(sceneObjects_, "externals/navimap/stage.navmesh");
				NavMesh* navMesh = navMeshManager->GetNavMesh();
				if (enemy_ && navMesh) {
					enemy_->SetNavMesh(navMesh);
					AddNavMeshLog("NavMesh re-set to Enemy");
				}

				// 可視化が有効な場合は次のフレームで更新
				if (engine->IsNavVis()) {
					engine->RequestNavVisUpdate();
					AddNavMeshLog("NavMesh visualization will update next frame");
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Load NavMesh")) {
				ClearNavMeshLogs();
				const std::string navMeshPath = "externals/navimap/stage.navmesh";
				if (engine->LoadNavMesh(navMeshPath)) {
					NavMesh* navMesh = navMeshManager->GetNavMesh();
					if (enemy_ && navMesh) {
						enemy_->SetNavMesh(navMesh);
					}
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Clear Logs")) {
				ClearNavMeshLogs();
			}

			ImGui::Separator();

			// NavMesh情報表示
			NavMesh* navMesh = navMeshManager->GetNavMesh();
			if (navMesh) {
				ImGui::Text("NavMesh Status: %s", navMesh->IsValid() ? "Valid" : "Invalid");

				// シーンオブジェクト数を表示
				ImGui::Text("Scene Objects: %d", static_cast<int>(sceneObjects_.size()));

				if (ImGui::CollapsingHeader("Scene Objects Details")) {
					int idx = 0;
					for (const auto& obj : sceneObjects_) {
						Model* model = obj->GetModel();
						if (model) {
							const ModelData& modelData = model->GetModelData();
							ImGui::Text("Object %d: %d vertices, %d triangles",
								idx++,
								static_cast<int>(modelData.vertices.size()),
								static_cast<int>(modelData.indices.size()) / 3);
						}
					}
				}
			}
			else {
				ImGui::Text("NavMesh: Not Initialized");
			}
		}

		// Enemy AI Debug情報
		if (ImGui::CollapsingHeader("Enemy AI Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (enemy_) {
				ImGui::Text("Enemy Position: (%.2f, %.2f, %.2f)",
					enemy_->GetPosition().x,
					enemy_->GetPosition().y,
					enemy_->GetPosition().z);

				ImGui::Text("Is Chasing: %s", enemy_->IsChasing() ? "Yes" : "No");
			}
			else {
				ImGui::Text("Enemy: Not Initialized");
			}
		}

		ImGui::Separator();

		// ログ表示
		if (ImGui::CollapsingHeader("NavMesh Logs", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::BeginChild("LogScrolling", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
			for (const auto& log : navMeshLogs_) {
				// エラーは赤、成功は緑、それ以外は白
				if (log.find("ERROR") != std::string::npos) {
					ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", log.c_str());
				}
				else if (log.find("SUCCESS") != std::string::npos) {
					ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", log.c_str());
				}
				else if (log.find("===") != std::string::npos) {
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "%s", log.c_str());
				}
				else {
					ImGui::Text("%s", log.c_str());
				}
			}
			// 自動スクロール
			if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}
			ImGui::EndChild();
		}

		ImGui::End();
	}
#endif
}

void GamePlayScene::Finalize() {
	// BGMを停止
	UnoEngine* engine = UnoEngine::GetInstance();
	if (engine && !sceneData_.audio.bgm.name.empty()) {
		engine->StopAudio(sceneData_.audio.bgm.name);
	}

	if (player_) {
		player_->Finalize();
		player_.reset();
	}
	if (enemy_) {
		enemy_->Finalize();
		enemy_.reset();
	}

	// Orbのクリーンアップ
	for (auto& orb : orbs_) {
		if (orb) {
			orb->Finalize();
		}
	}
	orbs_.clear();

	sceneObjects_.clear();
	skybox_.reset();
	lightManager_.reset();
	fpsCamera_.reset();
	postProcess_.reset();
	fadeSprite_.reset();
}

void GamePlayScene::AddNavMeshLog(const std::string& message) {
	navMeshLogs_.push_back(message);
	// 最大1000行まで保持
	if (navMeshLogs_.size() > 1000) {
		navMeshLogs_.erase(navMeshLogs_.begin());
	}
}

void GamePlayScene::ClearNavMeshLogs() {
	navMeshLogs_.clear();
}

void GamePlayScene::HandleInput() {
	UnoEngine* engine = UnoEngine::GetInstance();

	if (engine->IsKeyTrig(DIK_F)) {
		lightManager_->ToggleDebugDisplay();
	}

	if (engine->IsKeyTrig(DIK_M)) {
		showNavMeshDebug_ = !showNavMeshDebug_;
	}

	if (engine->IsKeyTrig(DIK_V)) {
		if (fpsCamera_) {
			bool currentMode = fpsCamera_->IsFPSMode();
			fpsCamera_->SetFPSMode(!currentMode);
		}
	}

	if (engine->IsKeyTrig(DIK_TAB)) {
		// TABキーでマウス固定/非固定を切り替え
		if (fpsCamera_) {
			fpsCamera_->ToggleMouseLook();
		}
	}

	// F4キーで魚眼レンズのON/OFF切り替え
	if (engine->IsKeyTrig(DIK_F4)) {
		static bool fisheyeEnabled = true;
		fisheyeEnabled = !fisheyeEnabled;
		if (fisheyeEnabled) {
			fisheyeStrength_ = 2.58f;  // デフォルト値に戻す
			OutputDebugStringA("Fisheye lens: ON\n");
		} else {
			fisheyeStrength_ = 0.0f;   // 魚眼レンズを無効化
			OutputDebugStringA("Fisheye lens: OFF\n");
		}
	}

	// R キーでナビメッシュ再生成
	if (engine->IsKeyTrig(DIK_R)) {
		ClearNavMeshLogs();
		AddNavMeshLog("=== Regenerating NavMesh (R key) ===");
		engine->GenNav(sceneObjects_, "externals/navimap/stage.navmesh");

		// 可視化が有効な場合は次のフレームで更新
		if (engine->IsNavVis()) {
			engine->RequestNavVisUpdate();
			AddNavMeshLog("NavMesh visualization will update next frame");
		}
	}
}

void GamePlayScene::UpdateRespawn(float deltaTime) {
	// リスポーン処理中でない場合は何もしない
	if (respawnState_ == RespawnState::None) {
		return;
	}

	respawnTimer_ += deltaTime;

	switch (respawnState_) {
	case RespawnState::FadeOut:
		// 暗転開始
		fadeAlpha_ = respawnTimer_ / FADE_DURATION;
		if (fadeAlpha_ >= 1.0f) {
			fadeAlpha_ = 1.0f;
			// 完全に暗転したら位置をリセット
			ResetPositions();
			respawnState_ = RespawnState::Respawning;
			respawnTimer_ = 0.0f;
		}
		break;

	case RespawnState::Respawning:
		// 少し待機(0.5秒)
		if (respawnTimer_ >= 0.5f) {
			respawnState_ = RespawnState::FadeIn;
			respawnTimer_ = 0.0f;
		}
		break;

	case RespawnState::FadeIn:
		// 明転開始
		fadeAlpha_ = 1.0f - (respawnTimer_ / FADE_DURATION);
		if (fadeAlpha_ <= 0.0f) {
			fadeAlpha_ = 0.0f;
			// 完全に明るくなったらリスポーン終了
			respawnState_ = RespawnState::None;
			respawnTimer_ = 0.0f;
		}
		break;
	}
}

void GamePlayScene::StartRespawn() {
	OutputDebugStringA("Starting respawn process...\n");
	respawnState_ = RespawnState::FadeOut;
	respawnTimer_ = 0.0f;
	fadeAlpha_ = 0.0f;
}

void GamePlayScene::ResetPositions() {
	OutputDebugStringA("Resetting player and enemy positions...\n");

	// プレイヤーを初期位置に戻す
	if (player_) {
		player_->SetPosition(playerInitialPos_);
		player_->SetJumpscareMode(false);  // 動けるようにする
		OutputDebugStringA(("Player reset to position: (" + 
			std::to_string(playerInitialPos_.x) + ", " +
			std::to_string(playerInitialPos_.y) + ", " +
			std::to_string(playerInitialPos_.z) + ")\n").c_str());
	}

	// エネミーを初期位置に戻して全ての状態をリセット
	if (enemy_) {
		enemy_->SetPosition(enemyInitialPos_);
		enemy_->ResetAIState();  // AI状態、BGM、アニメーション全てをリセット
		OutputDebugStringA(("Enemy reset to position: (" + 
			std::to_string(enemyInitialPos_.x) + ", " +
			std::to_string(enemyInitialPos_.y) + ", " +
			std::to_string(enemyInitialPos_.z) + ")\n").c_str());
	}

	// ジャンプスケアフラグをリセット
	jumpscareStarted_ = false;

	// ライティングを初期状態にリセット
	if (lightManager_) {
		lightManager_->Initialize();
		OutputDebugStringA("Lighting reset to initial state\n");
	}

	// カメラをプレイヤー位置に戻す
	if (camera_) {
		camera_->SetTranslate({playerInitialPos_.x, playerInitialPos_.y + 1.5f, playerInitialPos_.z - 5.0f});
		camera_->Update();
	}

	OutputDebugStringA("Position reset complete. Orbs preserved.\n");
}

