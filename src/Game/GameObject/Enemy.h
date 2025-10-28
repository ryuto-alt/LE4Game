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
	bool CheckWallAt(const Vector3& position);
	void UpdateNavMeshPath();
	void FollowPath();
	void ApplyAIConfig();

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

	// Audio
	SpatialAudioListener* audioListener_{nullptr};
	std::unique_ptr<SpatialAudioSource> footstepSource1_;
	std::unique_ptr<SpatialAudioSource> footstepSource2_;
	bool useFootstep1_{true};
	float lastAnimationTime_{0.0f};
	const float FOOTSTEP_INTERVAL = 0.3f;
};
