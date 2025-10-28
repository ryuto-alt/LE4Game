#pragma once
#include "UnoEngine.h"
#include "EnemyAIConfig.h"
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
	void Update();
	void Draw();
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

	// Collision response
	void HandleCollisionResponse();

private:
	void UpdateAnimation();
	void UpdateFootstepAudio();
	void UpdateDetectionSound();
	bool CheckWallAt(const Vector3& position);
	void UpdateNavMeshPath();
	void FollowPath();
	void ApplyAIConfig();
	void GenerateRandomExplorationPoint();
	void UpdateExploration();
	void CheckAndHandleStuck();
	void RecoverFromStuck();

	// 3D object and model
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<AnimatedModel> animatedModel_;

	// Transform
	Vector3 position_{0.0f, 0.0f, 0.0f};
	float currentRotationY_{0.0f};
	float targetRotationY_{0.0f};
	float currentSpeed_{0.0f};

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
	float detectionRange_{100.0f};
	float moveSpeed_{0.125f};
	bool isChasing_{false};

	// Wall avoidance (for UI/future use)
	float avoidanceRadius_{5.0f};
	float alternativeTimer_{0.0f};

	// NavMesh pathfinding
	NavMesh* navMesh_{nullptr};
	std::vector<Vector3> currentPath_;
	int currentWaypointIndex_{0};
	float pathUpdateTimer_{0.0f};
	float pathUpdateInterval_{0.5f};
	bool isAtCorner_{false};
	float cornerSlowdownFactor_{1.0f};

	// Exploration phase
	bool isExploring_{true};  // 探索モード中かどうか
	Vector3 explorationTarget_{0.0f, 0.0f, 0.0f};  // 探索目標地点
	float explorationIdleTimer_{0.0f};  // 目標到達後の待機タイマー
	const float EXPLORATION_IDLE_TIME = 2.0f;  // 到達後の待機時間
	const float EXPLORATION_ARRIVAL_THRESHOLD = 2.0f;  // 到達判定距離
	const float EXPLORATION_MIN_DISTANCE = 20.0f;  // プレイヤーからの最小距離
	const float EXPLORATION_MAX_DISTANCE = 60.0f;  // プレイヤーからの最大距離
	const float EXPLORATION_RANGE = 30.0f;  // 探索ポイント生成の範囲

	// Stack detection and recovery
	Vector3 previousPosition_{0.0f, 0.0f, 0.0f};  // 前フレームの位置
	float stuckTimer_{0.0f};  // スタック時間カウンター
	const float STUCK_DETECTION_TIME = 2.0f;  // スタック判定時間（2秒）
	const float STUCK_DISTANCE_THRESHOLD = 0.5f;  // スタック判定距離（0.5m以内）
	bool isRecoveringFromStuck_{false};  // スタック回避中フラグ

	// Audio
	SpatialAudioListener* audioListener_{nullptr};
	std::unique_ptr<SpatialAudioSource> footstepSource1_;
	std::unique_ptr<SpatialAudioSource> footstepSource2_;
	bool useFootstep1_{true};
	float lastAnimationTime_{0.0f};
	const float FOOTSTEP_INTERVAL = 0.3f;

	// Detection Sound
	std::unique_ptr<SpatialAudioSource> detectionSound_;
	float lastDetectionSoundEndTime_{-10.0f};  // 最後にサウンドが終了した時刻
	bool isDetectionSoundPlaying_{false};  // サウンドが再生中かどうか
	const float DETECTION_SOUND_COOLDOWN = 3.0f;  // 再生終了後3秒のクールタイム
	const float DETECTION_SOUND_RANGE = 20.0f;
};
