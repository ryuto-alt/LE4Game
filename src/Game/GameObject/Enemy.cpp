#include "Enemy.h"
#include "imgui.h"
#include <numbers>

Enemy::Enemy()
	: position_({0.0f, 0.0f, 0.0f})
	, currentRotationY_(0.0f)
	, animationPaused_(false)
	, isBlending_(false)
	, blendTimer_(0.0f)
	, animationEnabled_(true) {
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

	// Playerと同じ: アニメーションを変更して再生
	animatedModel_->ChangeAnimation("Walk");
	animatedModel_->PlayAnimation();

	// Object3Dの作成 - Playerと全く同じ順序
	object3d_ = engine->CreateObject3D();
	object3d_->SetModel(static_cast<Model*>(animatedModel_.get()));
	object3d_->SetAnimatedModel(animatedModel_.get());
	object3d_->SetPosition(position_);
	object3d_->SetScale(Vector3{1.0f, 1.0f, 1.0f});
	object3d_->SetRotation(Vector3{0.0f, 3.14f, 0.0f}); // Playerと同じY軸180度回転
	object3d_->SetEnableLighting(true);
	object3d_->SetEnableAnimation(true);
	object3d_->SetCamera(camera);

	// 環境マップを無効化（Playerと同じ）
	object3d_->EnableEnv(false);

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

	OutputDebugStringA("Enemy: Initialization complete with Walk animation\n");
}

void Enemy::Update() {
	// アニメーションの更新
	UpdateAnimation();

	// オブジェクトの位置と回転を更新
	if (object3d_) {
		object3d_->SetPosition(position_);
		object3d_->SetRotation(Vector3{0.0f, currentRotationY_, 0.0f});
		object3d_->Update();
	}
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

	// 簡易的な情報表示（Run/Screamは未実装）
	ImGui::Separator();
	ImGui::Text("Note: Only Walk animation is currently loaded");

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
		ImGui::Text("Current: Walk");
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
