#pragma once
#include "UnoEngine.h"
#include <memory>
#include <vector>

// Forward declaration
class Player;
class NavMesh;

class Enemy {
public:
	Enemy();
	~Enemy();

	void Initialize(Camera* camera = nullptr);
	void Update();
	void Draw();
	void DrawUI();
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

	// 3D object and model
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<AnimatedModel> animatedModel_;

	// Transform
	Vector3 position_;
	float currentRotationY_;

	// Animation
	bool animationPaused_;
	bool isBlending_;
	float blendTimer_;
	const float BLEND_DURATION = 0.3f;

	// Animation toggle for ImGui
	bool animationEnabled_;

	// Current animation index for ImGui combo
	int currentAnimationIndex_;

	// Player tracking
	Player* player_;
	float detectionRange_;
	float moveSpeed_;
	bool isChasing_;

	// Wall avoidance (for UI/future use)
	float avoidanceRadius_;
	float alternativeTimer_;

	// NavMesh pathfinding
	NavMesh* navMesh_;
	std::vector<Vector3> currentPath_;
	int currentWaypointIndex_;
	float pathUpdateTimer_;
	const float PATH_UPDATE_INTERVAL = 0.5f; // 0.5秒ごとにパス更新

	// Audio
	SpatialAudioListener* audioListener_;
	std::unique_ptr<SpatialAudioSource> footstepSource1_;
	std::unique_ptr<SpatialAudioSource> footstepSource2_;
	bool useFootstep1_;  // true: footstep1を使用, false: footstep2を使用
	float lastAnimationTime_;  // アニメーション時間の記録
	const float FOOTSTEP_INTERVAL = 0.5f;  // 足音の間隔（秒）
};
