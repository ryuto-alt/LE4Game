#pragma once
#include "UnoEngine.h"
#include "EnemyAIConfig.h"
#include "LineRenderer.h"
#include <memory>
#include <vector>

// Forward declaration
class Player;
class NavMesh;

class Enemy {
public:
	Enemy();
	~Enemy();

	void Initialize(Camera* camera = nullptr, const EnemyAIConfig& aiConfig = EnemyAIConfig{});
	void Update(UnoEngine* engine);
	void Draw();
	void DrawDebugVision();  // 視界デバッグ描画
	void Finalize();

	// Position
	Vector3 GetPosition() const { return position_; }
	void SetPosition(const Vector3& position);

	// Animation control
	void PauseAnimation();
	void PlayAnimation();
	void ResetAnimation();
	bool IsAnimationPaused() const { return animationPaused_; }
	std::string GetCurrentAnimationName() const;
	void ChangeAnimation(const std::string& animationName);

	// Lighting
	void SetDirectionalLight(DirectionalLight* light);
	void SetSpotLight(SpotLight* light);

	// Environment mapping
	void EnableEnv(bool enable);
	bool IsEnvEnabled() const;
	void SetEnvTex(const std::string& texturePath);

	// Camera
	void SetCamera(Camera* camera);

	// Audio
	void SetAudioListener(SpatialAudioListener* listener) { audioListener_ = listener; }

	// Player tracking
	void SetPlayer(Player* player) { player_ = player; }

	// NavMesh
	void SetNavMesh(NavMesh* navMesh) { navMesh_ = navMesh; }

	// AI parameters
	void SetIntelligence(float value);
	void SetAggressiveness(float value);
	void SetMobility(float value);
	void SetAIConfig(const EnemyAIConfig& config);
	const EnemyAIConfig& GetAIConfig() const { return aiConfig_; }

	// Getters
	Object3d* GetObject() { return object3d_.get(); }
	AnimatedModel* GetModel() { return animatedModel_.get(); }
	bool IsChasing() const { return isChasing_; }
	Vector3 GetHeadPosition() const;  // 頭の位置を取得

	// Jumpscare
	void StartJumpscare();
	bool IsJumpscaring() const { return isJumpscaring_; }
	bool IsJumpscareFinished() const;
	void ResetJumpscare();  // ジャンプスケア状態をリセット
	void ResetAIState();  // AI状態を完全にリセット

	// Collision response
	void HandleCollisionResponse();

	// Debug
	bool debugDrawVision_{false};  // 視界デバッグ描画フラグ
	bool debugStopMovement_{false};  // デバッグ用：移動停止フラグ
#ifdef _DEBUG
	bool debugDrawFootBones_{true};  // 足のボーンデバッグ描画フラグ（DebugビルドのみデフォルトON）
#else
	bool debugDrawFootBones_{false};  // 足のボーンデバッグ描画フラグ（ReleaseビルドはOFF）
#endif
	float debugAnimationSpeed_{1.0f};  // アニメーション速度（1.0 = 通常速度）
	bool debugManualAnimationControl_{false};  // 手動アニメーション制御フラグ
	float debugManualAnimationTime_{0.0f};  // 手動アニメーション時間

private:
	void UpdateAnimation(float deltaTime);
	void UpdateFootstepAudio(float deltaTime);
	void UpdateDetectionSound();
	void UpdateBarkSound(float deltaTime);
	void UpdateChaseBGM(float deltaTime, UnoEngine* engine);
	bool CheckWallAt(const Vector3& position);
	void UpdateNavMeshPath();
	void FollowPath(float deltaTime);
	void ApplyAIConfig();
	void CheckAndHandleStuck(float deltaTime);
	void RecoverFromStuck();
	bool IsPlayerInVision();
	Vector3 GetRandomPatrolPoint();  // 徘徊用ランダムポイント取得
	void DrawFootBoneDebug();  // 足のボーンデバッグ描画

	// 恐怖演出用ヘルパー関数
	float CalculateHorrorVolume(float distance);  // 距離に応じた恐怖的な音量計算

	// 3D object and model
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<AnimatedModel> animatedModel_;

	// Transform
	Vector3 position_{0.0f, 0.0f, 0.0f};
	float currentRotationY_{0.0f};
	float targetRotationY_{0.0f};
	float currentSpeed_{0.0f};

	// Gravity and ground collision
	float velocityY_{0.0f};  // 垂直方向の速度
	bool isGrounded_{false};  // 地面に接地しているか
	const float GRAVITY = -0.5f;  // 重力加速度
	const float GROUND_HEIGHT = 0.0f;  // 地面の高さ
	const float GROUND_CHECK_OFFSET = 0.1f;  // 地面判定のオフセット

	// Animation
	bool animationPaused_{false};
	bool isBlending_{false};
	float blendTimer_{0.0f};
	const float BLEND_DURATION = 0.3f;

	// Animation toggle for ImGui
	bool animationEnabled_{true};

	// Current animation index for ImGui combo
	int currentAnimationIndex_{0};

	// AI Config
	EnemyAIConfig aiConfig_;

	// Player tracking
	Player* player_{nullptr};
	float moveSpeed_{8.0f};  // 1秒あたり8ユニット（60FPSで約0.133/frame）
	float patrolMoveSpeed_{4.5f};  // 徘徊時の移動速度（EnemyAIConfigから設定）
	float searchMoveSpeed_{5.5f};  // 捜索時の移動速度（音検知後）
	bool isChasing_{false};
	bool wasChasing_{false};  // 前フレームの追跡状態（追跡開始検出用）

	// Sound detection
	float soundDetectionRange_{30.0f};  // 音検知範囲（デフォルト30m）
	Vector3 lastHeardSoundPosition_{0, 0, 0};  // 最後に聞いた音の位置
	float lastSoundTime_{-999.0f};  // 最後に音を聞いた時刻
	const float SOUND_REACTION_TIME = 0.5f;  // 音に反応する時間閾値

	// Vision-based detection
	const float VISION_RANGE = 30.0f;  // 視界範囲30m
	const float VISION_ANGLE = 90.0f;  // 視野角±90度（合計180度）
	const float VISION_DETECTION_DISTANCE = 10.0f;  // 視界検知距離15m
	const float CHASE_RELEASE_DISTANCE = 25.0f;  // 追跡解除距離25m（より遠くまで追跡）
	const float LOST_SIGHT_GRACE_PERIOD = 10.0f;  // 視界を失ってから追跡を続ける時間（10秒に延長）

	// Chase persistence
	Vector3 lastSeenPlayerPosition_{0, 0, 0};  // 最後に見たプレイヤーの位置
	float lostSightTimer_{0.0f};  // 視界を失ってからの経過時間
	bool isSearching_{false};  // 捜索モード（音検知後、視認前）

	// Wall avoidance (for UI/future use)
	float avoidanceRadius_{5.0f};
	float alternativeTimer_{0.0f};

	// NavMesh pathfinding (old system)
	NavMesh* navMesh_{nullptr};
	std::vector<Vector3> currentPath_;
	int currentWaypointIndex_{0};
	float pathUpdateTimer_{0.0f};
	float pathUpdateInterval_{0.5f};
	bool isAtCorner_{false};
	float cornerSlowdownFactor_{1.0f};


	// Stack detection and recovery
	Vector3 previousPosition_{0.0f, 0.0f, 0.0f};  // 前フレームの位置
	float stuckTimer_{0.0f};  // スタック時間カウンター
	const float STUCK_DETECTION_TIME = 2.5f;  // スタック判定時間（1.5秒に短縮）
	const float STUCK_DISTANCE_THRESHOLD = 0.3f;  // スタック判定距離（より敏感に）
	bool isRecoveringFromStuck_{false};  // スタック回避中フラグ
	int stuckRecoveryAttempts_{0};  // スタック回避試行回数

	// Audio
	SpatialAudioListener* audioListener_{nullptr};
	std::unique_ptr<SpatialAudioSource> footstepSource1_;
	std::unique_ptr<SpatialAudioSource> footstepSource2_;
	bool useFootstep1_{true};
	float lastAnimationTime_{0.0f};
	const float FOOTSTEP_INTERVAL = 0.25f;

	// Foot bone tracking for footstep sounds
	float previousLeftFootY_{0.0f};
	float previousRightFootY_{0.0f};
	bool leftFootWasAboveGround_{true};  // 初期値をtrueに（最初は空中と仮定）
	bool rightFootWasAboveGround_{true};  // 初期値をtrueに（最初は空中と仮定）
	const float FOOT_GROUND_THRESHOLD = 0.15f;  // 地面判定の閾値（低めに設定して足の上げ下げを検出）
	const float FOOT_LIFT_THRESHOLD = 0.25f;  // 足が上がったと判定する閾値（これ以上上がったら次の着地検出可能）
	float lastLeftFootLandTime_{-999.0f};  // 最後に左足が着地した時間
	float lastRightFootLandTime_{-999.0f};  // 最後に右足が着地した時間
	const float FOOTSTEP_COOLDOWN = 0.2f;  // 足音のクールダウン時間（秒）- 交互検出を強制（1.5倍速対応）
	enum class LastFootLanded { None, Left, Right };
	LastFootLanded lastFootLanded_{LastFootLanded::None};  // 最後に着地した足

	// Detection Sound
	std::unique_ptr<SpatialAudioSource> detectionSound_;
	float lastDetectionSoundEndTime_{-10.0f};  // 最後にサウンドが終了した時刻
	bool isDetectionSoundPlaying_{false};  // サウンドが再生中かどうか
	const float DETECTION_SOUND_COOLDOWN = 3.0f;  // 再生終了後3秒のクールタイム
	const float DETECTION_SOUND_RANGE = 20.0f;

	// Bark Sound (during chase)
	std::unique_ptr<SpatialAudioSource> barkSound_;
	float lastBarkTime_{-999.0f};  // 最後に吠えた時刻
	float nextBarkInterval_{10.0f};  // 次の吠え声までの間隔（ランダムに変化）
	const float BARK_MIN_INTERVAL = 8.0f;  // 吠え声の最小間隔（秒）
	const float BARK_MAX_INTERVAL = 12.0f;  // 吠え声の最大間隔（秒）

	// Chase BGM
	bool chaseBGMLoaded_{false};  // chaseBGMがロード済みかどうか
	bool chaseBGMPlaying_{false};  // chaseBGMが再生中かどうか
	float chaseBGMVolume_{0.0f};  // 現在のchaseBGMボリューム
	float chaseBGMTargetVolume_{0.0f};  // 目標ボリューム
	bool isFadingIn_{false};  // フェードイン中かどうか
	bool isFadingOut_{false};  // フェードアウト中かどうか
	const float CHASE_BGM_MAX_VOLUME = 0.3f;  // chaseBGMの最大ボリューム
	const float FADE_IN_DURATION = 2.0f;  // フェードイン時間（秒）
	const float FADE_OUT_DURATION = 3.0f;  // フェードアウト時間（秒）

	// Foot debug visualization
	std::unique_ptr<LineRenderer> footDebugLineRenderer_;

	// Jumpscare
	bool isJumpscaring_{false};
	float jumpscareTimer_{0.0f};
	float jumpscareDuration_{0.0f};
};
