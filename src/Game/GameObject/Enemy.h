#pragma once
#include "UnoEngine.h"
#include <memory>

// Forward declaration
class Player;

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

	// Player tracking
	void SetPlayer(Player* player) { player_ = player; }

	// Getters
	Object3d* GetObject() { return object3d_.get(); }
	AnimatedModel* GetModel() { return animatedModel_.get(); }

	// Collision response
	void HandleCollisionResponse();

private:
	void UpdateAnimation();
	bool CheckWallAt(const Vector3& position);

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
};
