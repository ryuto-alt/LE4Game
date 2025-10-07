// src/Engine/Graphics/Object3d.cpp
#include "Object3d.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Mymath.h"
#include "TextureManager.h"
#include "Animation.h"
#include "AnimatedModel.h"
#include "AnimationUtility.h"
#include "UnoEngine.h"
#include "AABBCollision.h"
#include <unordered_set>


Object3d::Object3d() : model_(nullptr), dxCommon_(nullptr), spriteCommon_(nullptr),
materialData_(nullptr), transformationMatrixData_(nullptr), directionalLightData_(nullptr),
spotLightData_(nullptr), cameraData_(nullptr), camera_(nullptr) {
	// 初期値設定
	transform_.scale = { 1.0f, 1.0f, 1.0f };
	transform_.rotate = { 0.0f, 0.0f, 0.0f };
	transform_.translate = { 0.0f, 0.0f, 0.0f };

	// アニメーション行列を単位行列で初期化
	animationMatrix_ = MakeIdentity4x4();
}

Object3d::~Object3d() {
	// ComPtrリソースの解放（Unmapは不要 - これらは永続的にマップされている）
	if (materialResource_) {
		materialResource_.Reset();
	}
	if (transformationMatrixResource_) {
		transformationMatrixResource_.Reset();
	}
	if (directionalLightResource_) {
		directionalLightResource_.Reset();
	}
	if (spotLightResource_) {
		spotLightResource_.Reset();
	}
	if (cameraResource_) {
		cameraResource_.Reset();
	}

	// データポインタをnullptrに設定（安全のため）
	materialData_ = nullptr;
	transformationMatrixData_ = nullptr;
	directionalLightData_ = nullptr;
	spotLightData_ = nullptr;
	cameraData_ = nullptr;
}

void Object3d::Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon) {
	assert(dxCommon);
	assert(spriteCommon);
	dxCommon_ = dxCommon;
	spriteCommon_ = spriteCommon;

	// マテリアルリソースの作成
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
	// マテリアルデータの書き込み
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	// デフォルトカラーを白に設定（モデルのマテリアルで上書きされる）
	// PBRマテリアルの初期化
	materialData_->baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->metallicFactor = 0.0f;
	materialData_->roughnessFactor = 1.0f;
	materialData_->normalScale = 1.0f;
	materialData_->occlusionStrength = 1.0f;
	materialData_->emissiveFactor = { 0.0f, 0.0f, 0.0f };
	materialData_->alphaCutoff = 0.5f;
	materialData_->hasBaseColorTexture = 0;
	materialData_->hasMetallicRoughnessTexture = 0;
	materialData_->hasNormalTexture = 0;
	materialData_->hasOcclusionTexture = 0;
	materialData_->hasEmissiveTexture = 0;
	materialData_->enableLighting = 1;
	materialData_->alphaMode = 0; // OPAQUE
	materialData_->doubleSided = 0;
	materialData_->enableEnvironmentMap = 0; // デフォルトでオフ
	materialData_->environmentMapIntensity = 0.3f; // デフォルト強度
	materialData_->uvTransform = MakeIdentity4x4();

	// 変換行列リソースの作成
	transformationMatrixResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	// 変換行列データの書き込み
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();

	// ライトリソースの作成
	directionalLightResource_ = dxCommon_->CreateBufferResource(sizeof(DirectionalLight));
	// ライトデータの書き込み
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData_->intensity = 1.0f;

	// スポットライトリソースの作成
	spotLightResource_ = dxCommon_->CreateBufferResource(sizeof(SpotLight));
	// スポットライトデータの書き込み
	spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));
	spotLightData_->color = { 1.0f, 1.0f, 0.9f, 1.0f };  // 暖かい白色
	spotLightData_->position = { 0.0f, 5.0f, 0.0f };     // デフォルト位置（上から）
	spotLightData_->intensity = 2.0f;                     // 強めの光
	spotLightData_->direction = { 0.0f, -1.0f, 0.0f };   // 下向き
	spotLightData_->innerCone = cosf(12.0f * 3.14159265f / 180.0f);  // 内側コーン角12度
	spotLightData_->attenuation = { 1.0f, 0.09f, 0.032f };           // 減衰パラメータ
	spotLightData_->outerCone = cosf(20.0f * 3.14159265f / 180.0f);  // 外側コーン角20度
	
	// カメラデータリソースの作成
	cameraResource_ = dxCommon_->CreateBufferResource(sizeof(CameraData));
	// カメラデータの書き込み
	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
	cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
	cameraData_->fisheyeStrength = 0.0f;  // 初期値は魚眼レンズオフ
	
	// デフォルトテクスチャを事前にロードして描画中の動的SRV作成を避ける
	TextureManager::GetInstance()->LoadDefaultTexture();
	
	// デフォルト環境マップテクスチャを事前にロード
	std::string defaultEnvMap = "Resources/Models/skybox/warm_restaurant_night_2k.hdr";
	DWORD envMapAttribs = GetFileAttributesA(defaultEnvMap.c_str());
	if (envMapAttribs != INVALID_FILE_ATTRIBUTES) {
		TextureManager::GetInstance()->LoadTexture(defaultEnvMap);
		OutputDebugStringA(("Object3d::Initialize - Loaded default environment map: " + defaultEnvMap + "\n").c_str());
	}
	else {
		OutputDebugStringA(("Object3d::Initialize - WARNING: Default environment map not found: " + defaultEnvMap + "\n").c_str());
	}
}

void Object3d::SetModel(Model* model) {
	model_ = model;

	OutputDebugStringA("Object3d::SetModel - Called\n");
	OutputDebugStringA(("Object3d::SetModel - model_ is " + std::string(model_ ? "not null" : "null") + "\n").c_str());
	OutputDebugStringA(("Object3d::SetModel - materialData_ is " + std::string(materialData_ ? "not null" : "null") + "\n").c_str());

	// モデルのマテリアル情報をシェーダーに設定
	if (model_ && materialData_) {
		OutputDebugStringA("Object3d::SetModel - Applying material data\n");
		const MaterialData& modelMaterial = model_->GetMaterial();

		// PBRマテリアルデータをシェーダーのMaterial構造体に反映
		if (modelMaterial.isPBR) {
			// PBRモード: baseColorFactorとPBRプロパティを使用
			materialData_->baseColorFactor = modelMaterial.baseColorFactor;
			materialData_->metallicFactor = modelMaterial.metallicFactor;
			materialData_->roughnessFactor = modelMaterial.roughnessFactor;
			materialData_->normalScale = modelMaterial.normalScale;
			materialData_->occlusionStrength = modelMaterial.occlusionStrength;
			materialData_->emissiveFactor = modelMaterial.emissiveFactor;
			materialData_->alphaCutoff = modelMaterial.alphaCutoff;
			
			// テクスチャフラグの設定
			materialData_->hasBaseColorTexture = !modelMaterial.textureFilePath.empty() ? 1 : 0;
			materialData_->hasMetallicRoughnessTexture = 0;
			materialData_->hasNormalTexture = 0;
			materialData_->hasOcclusionTexture = 0;
			materialData_->hasEmissiveTexture = 0;
			materialData_->enableLighting = 1;

			// アルファモード変換
			if (modelMaterial.alphaMode == "MASK") {
				materialData_->alphaMode = 1;
			} else if (modelMaterial.alphaMode == "BLEND") {
				materialData_->alphaMode = 2;
			} else {
				materialData_->alphaMode = 0; // OPAQUE
			}

			materialData_->doubleSided = modelMaterial.doubleSided ? 1 : 0;
			materialData_->enableEnvironmentMap = enableEnvironmentMap_ ? 1 : 0;
			
			OutputDebugStringA("Object3d::SetModel - Applied PBR material data\n");
			char pbrDebugMsg[512];
			sprintf_s(pbrDebugMsg, "  BaseColor: R=%.2f, G=%.2f, B=%.2f, A=%.2f\n  Metallic=%.2f, Roughness=%.2f\n  HasTexture=%d, EnableLighting=%d\n",
				materialData_->baseColorFactor.x, materialData_->baseColorFactor.y, 
				materialData_->baseColorFactor.z, materialData_->baseColorFactor.w,
				materialData_->metallicFactor, materialData_->roughnessFactor,
				materialData_->hasBaseColorTexture, materialData_->enableLighting);
			OutputDebugStringA(pbrDebugMsg);
		} else {
			// 従来モード: diffuseカラーをbaseColorFactorとして使用
			materialData_->baseColorFactor = modelMaterial.diffuse;
			materialData_->baseColorFactor.w = modelMaterial.alpha;
			materialData_->metallicFactor = 0.0f;
			materialData_->roughnessFactor = 1.0f;
			materialData_->hasBaseColorTexture = !modelMaterial.textureFilePath.empty() ? 1 : 0;
			materialData_->hasMetallicRoughnessTexture = 0;
			materialData_->hasNormalTexture = 0;
			materialData_->hasOcclusionTexture = 0;
			materialData_->hasEmissiveTexture = 0;
			materialData_->enableLighting = 1;
			materialData_->alphaMode = 0; // OPAQUE
			materialData_->doubleSided = 0;
			materialData_->enableEnvironmentMap = enableEnvironmentMap_ ? 1 : 0;
			OutputDebugStringA("Object3d::SetModel - Applied legacy material data\n");
		}


		Vector3 physicalScale = model_->GetModelData().rootTransform.scale;

		// physicalScaleが0の場合は1.0を使用（初期化されていない場合の対策）
		if (physicalScale.x == 0.0f) physicalScale.x = 1.0f;
		if (physicalScale.y == 0.0f) physicalScale.y = 1.0f;
		if (physicalScale.z == 0.0f) physicalScale.z = 1.0f;

		// Blenderタイリング効果
		Vector3 scale = {
			modelMaterial.textureScale.x * physicalScale.x,
			modelMaterial.textureScale.y * physicalScale.y,
			1.0f
		};
		Vector3 translate = { modelMaterial.textureOffset.x, modelMaterial.textureOffset.y, 0.0f };

		// デバッグ
		char debugMsg[512];
		sprintf_s(debugMsg, "Object3d::SetModel - Physical scale: X=%.3f, Y=%.3f, Z=%.3f\n"
			"  Original texture scale: X=%.3f, Y=%.3f\n"
			"  Final texture scale: X=%.3f, Y=%.3f\n",
			physicalScale.x, physicalScale.y, physicalScale.z,
			modelMaterial.textureScale.x, modelMaterial.textureScale.y,
			scale.x, scale.y);
		OutputDebugStringA(debugMsg);

		Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
		Matrix4x4 rotationMatrix = MakeIdentity4x4(); 
		Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

		//タイリング処理
		materialData_->uvTransform = Multiply(translateMatrix, Multiply(rotationMatrix, scaleMatrix));
		

		OutputDebugStringA(("Object3d::SetModel - Applied texture scale: " +
			std::to_string(modelMaterial.textureScale.x) + ", " +
			std::to_string(modelMaterial.textureScale.y) + "\n").c_str());
		OutputDebugStringA(("Object3d::SetModel - Applied texture offset: " +
			std::to_string(modelMaterial.textureOffset.x) + ", " +
			std::to_string(modelMaterial.textureOffset.y) + "\n").c_str());

		//materialData_->uvTransform = Multiply(translateMatrix, Multiply(rotationMatrix, scaleMatrix));
		// モデルのテクスチャパスの確認
		std::string texturePath = model_->GetTextureFilePath();
		OutputDebugStringA(("Object3d::SetModel - Model texture path: " + texturePath + "\n").c_str());

		if (!texturePath.empty()) {
			// テクスチャが未ロードなら読み込む
			if (!TextureManager::GetInstance()->IsTextureExists(texturePath)) {
				OutputDebugStringA(("Object3d::SetModel - Loading texture: " + texturePath + "\n").c_str());
				TextureManager::GetInstance()->LoadTexture(texturePath);
			}
			else {
				OutputDebugStringA(("Object3d::SetModel - Texture already loaded: " + texturePath + "\n").c_str());
			}
		}
		else {
			OutputDebugStringA("Object3d::SetModel - No texture path provided by model\n");
		}

		// デバッグ情報
		OutputDebugStringA("Object3d::SetModel - Material information:\n");
		OutputDebugStringA(("  - Model Material Diffuse (RGBA): " +
			std::to_string(modelMaterial.diffuse.x) + ", " +
			std::to_string(modelMaterial.diffuse.y) + ", " +
			std::to_string(modelMaterial.diffuse.z) + ", " +
			std::to_string(modelMaterial.diffuse.w) + "\n").c_str());
		OutputDebugStringA(("  - Applied to Shader (RGBA): " +
			std::to_string(materialData_->baseColorFactor.x) + ", " +
			std::to_string(materialData_->baseColorFactor.y) + ", " +
			std::to_string(materialData_->baseColorFactor.z) + ", " +
			std::to_string(materialData_->baseColorFactor.w) + "\n").c_str());
		OutputDebugStringA(("  - Texture: " + (texturePath.empty() ? "None" : texturePath) + "\n").c_str());
	}
	else {
		OutputDebugStringA("Object3d::SetModel - materialData_ is null, cannot apply material\n");
		if (!materialData_) {
			OutputDebugStringA("Object3d::SetModel - ERROR: materialData_ is null - Object3d may not be properly initialized\n");
		}
	}
}

// 従来のUpdateメソッド（ビュー行列とプロジェクション行列を直接指定）
void Object3d::Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
	assert(transformationMatrixData_);

	// ワールド行列の計算
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

	// アニメーション行列とワールド行列を合成
	Matrix4x4 finalWorldMatrix = Multiply(animationMatrix_, worldMatrix);

	// WVP行列の計算
	Matrix4x4 worldViewProjectionMatrix = Multiply(finalWorldMatrix, Multiply(viewMatrix, projectionMatrix));

	// 行列の更新
	transformationMatrixData_->WVP = worldViewProjectionMatrix;
	transformationMatrixData_->World = finalWorldMatrix;
}

// カメラセッター
void Object3d::SetCamera(Camera* camera) {
	camera_ = camera;
}

// カメラゲッター
Camera* Object3d::GetCamera() const {
	return camera_;
}

// 新しいUpdateメソッド（カメラを使用）
void Object3d::Update() {
	assert(transformationMatrixData_);

	// アニメーション処理
	if (animatedModel_) {
		Skeleton& skeleton = animatedModel_->GetSkeleton();
		SkinCluster& skinCluster = animatedModel_->GetSkinCluster();

		// デバッグ出力用
		static int frameCount = 0;

		// ブレンド対応のアニメーション適用
		if (animatedModel_->IsBlending()) {
			// ブレンド中は各ジョイントに対してブレンドされた変換を適用
			for (Joint& joint : skeleton.joints) {
				// 初期ジョイント変換を使用（前フレームの値ではなく）
				JointTransform originalTransform;
				auto initialIt = animatedModel_->GetInitialJointTransforms().find(joint.name);
				if (initialIt != animatedModel_->GetInitialJointTransforms().end()) {
					originalTransform = initialIt->second;
				}
				else {
					// 初期変換が見つからない場合は現在の値を使用
					originalTransform.scale = joint.transform.scale;
					originalTransform.rotate = joint.transform.rotate;
					originalTransform.translate = joint.transform.translate;
				}

				// ブレンドされた変換を取得
				auto blendedTransform = animatedModel_->GetBlendedTransform(joint.name, originalTransform);

				// デバッグ: スケール値を確認
				if (frameCount % 60 == 0 && joint.name == skeleton.joints[0].name) {
					OutputDebugStringA(("Object3d::Update - Joint " + joint.name +
						" original scale: (" + std::to_string(originalTransform.scale.x) + ", " +
						std::to_string(originalTransform.scale.y) + ", " +
						std::to_string(originalTransform.scale.z) + ")" +
						" blended scale: (" + std::to_string(blendedTransform.scale.x) + ", " +
						std::to_string(blendedTransform.scale.y) + ", " +
						std::to_string(blendedTransform.scale.z) + ")\n").c_str());
				}

				joint.transform.scale = blendedTransform.scale;
				joint.transform.rotate = blendedTransform.rotate;
				joint.transform.translate = blendedTransform.translate;
			}
		}
		else {
			// 通常のアニメーション適用
			const Animation& currentAnimation = animatedModel_->GetAnimationPlayer().GetAnimation();
			float animationTime = animatedModel_->GetAnimationPlayer().GetTime();
			ApplyAnimation(skeleton, currentAnimation, animationTime);
		}

		SkeletonUpdate(skeleton);
		SkinClusterUpdate(skinCluster, skeleton);

		// アニメーション行列は単位行列のままにする（スキニングで頂点変換するため）
		animationMatrix_ = MakeIdentity4x4();

		// アニメーション時刻の管理はAnimationPlayerに任せる
		// Object3d側では時刻を更新しない
	}
	else {
		// アニメーションデータが存在しない場合
		static int noAnimCount = 0;
		if (noAnimCount % 60 == 0) {
			// OutputDebugStringA(("Object3d::Update - No animation data available\n"));
		}
		noAnimCount++;
	}

	// カメラが設定されている場合のみ処理
	if (!camera_) {
		// カメラが設定されていない場合は何もしない
		//OutputDebugStringA("Object3d::Update - Camera is null, skipping matrix update\n");
		return;
	}

	Camera* useCamera = camera_;

	// ワールド行列の計算
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

	// アニメーション行列とワールド行列を合成
	Matrix4x4 finalWorldMatrix = Multiply(animationMatrix_, worldMatrix);

	// WVP行列の計算（カメラからビュープロジェクション行列を取得）
	Matrix4x4 worldViewProjectionMatrix = Multiply(finalWorldMatrix, useCamera->GetViewProjectionMatrix());

	// 行列の更新
	transformationMatrixData_->WVP = worldViewProjectionMatrix;
	transformationMatrixData_->World = finalWorldMatrix;
	
	// カメラの位置情報を更新
	if (cameraData_ && camera_) {
		cameraData_->worldPosition = camera_->GetTranslate();
		cameraData_->fisheyeStrength = camera_->GetFisheyeStrength();
	}
	
	// 環境マップの有効/無効を更新（PBR実装では使用しない）
	// PBRでは環境マップは自動的に処理される
}

void Object3d::Draw() {
	assert(dxCommon_);
	assert(model_);

	// パイプラインの設定（PBRとアニメーションの組み合わせに対応）
	bool usePBR = false;
	if (model_) {
		const MaterialData& material = model_->GetMaterial();
		usePBR = material.isPBR;
	}
	
	bool useAnimation = enableAnimation_ && animatedModel_;
	
	if (usePBR && useAnimation) {
		dxCommon_->GetCommandList()->SetPipelineState(spriteCommon_->GetPBRSkinningPipelineState().Get());
	}
	else if (usePBR) {
		dxCommon_->GetCommandList()->SetPipelineState(spriteCommon_->GetPBRPipelineState().Get());
	}
	else if (useAnimation) {
		dxCommon_->GetCommandList()->SetPipelineState(spriteCommon_->GetSkinningPipelineState().Get());
	}
	else {
		dxCommon_->GetCommandList()->SetPipelineState(spriteCommon_->GetGraphicsPipelineState().Get());
	}
	
	// プリミティブトポロジーの設定
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// アニメーションモデルの場合、頂点バッファとインフルエンスバッファを設定
	// マルチマテリアルの場合は後でループ内で設定するため、ここではスキップ
	const ModelData& modelData = model_->GetModelData();
	bool isMultiMaterial = !modelData.matVertexData.empty();

	if (!isMultiMaterial) {
		if (useAnimation) {
			AnimatedModel* animModel = static_cast<AnimatedModel*>(animatedModel_);
			const SkinCluster& skinCluster = animModel->GetSkinCluster();

			// 頂点バッファとインフルエンスバッファを設定
			D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
				model_->GetVBView(),
				skinCluster.influenceBufferView
			};
			dxCommon_->GetCommandList()->IASetVertexBuffers(0, 2, vbvs);
		}
		else {
			// 通常の頂点バッファのみセット
			dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &model_->GetVBView());
		}
	}

	// マテリアルCBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// 変換行列CBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

	// テクスチャの場所を設定
	std::string texturePath = model_->GetTextureFilePath();

	// デバッグ: 最初の数フレームだけログ出力
	static int drawCallCount = 0;
	if (drawCallCount < 10) {
		OutputDebugStringA(("Object3d::Draw - Texture path from model: " + texturePath + "\n").c_str());
		drawCallCount++;
	}

	// テクスチャが空または存在しない場合の詳細なチェック
	if (texturePath.empty()) {
		// OutputDebugStringA("Object3d::Draw - Texture path is empty, using default texture\n");
		texturePath = TextureManager::GetInstance()->GetDefaultTexturePath();
	}
	else if (!TextureManager::GetInstance()->IsTextureExists(texturePath)) {
		// テクスチャが存在しない場合は即座にデフォルトテクスチャを使用（描画中の動的読み込みを回避）
		OutputDebugStringA(("Object3d::Draw - WARNING: Texture not found, using default: " + texturePath + "\n").c_str());
		texturePath = TextureManager::GetInstance()->GetDefaultTexturePath();
	}
	else {
		// OutputDebugStringA(("Object3d::Draw - Using valid texture: " + texturePath + "\n").c_str());
	}

	// テクスチャをセット
	if (drawCallCount < 10) {
		OutputDebugStringA(("Object3d::Draw - Binding texture: " + texturePath + "\n").c_str());
	}
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2,
		TextureManager::GetInstance()->GetSrvHandleGPU(texturePath));

	// グローバル環境マップテクスチャを使用
	std::string envTexturePath = globalEnvironmentTexturePath_;
	if (envTexturePath.empty() || !TextureManager::GetInstance()->IsTextureExists(envTexturePath)) {
		envTexturePath = "Resources/Models/skybox/warm_restaurant_night_2k.hdr";

		if (!TextureManager::GetInstance()->IsTextureExists(envTexturePath)) {
			TextureManager::GetInstance()->LoadTexture(envTexturePath);
		}
	}

	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(6,
		TextureManager::GetInstance()->GetSrvHandleGPU(envTexturePath));

	// ライトCBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	// スポットライトCBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(5, spotLightResource_->GetGPUVirtualAddress());
	
	// カメラデータCBufferの場所を設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(7, cameraResource_->GetGPUVirtualAddress());

	// パレットSRVの設定（アニメーション用）
	if (useAnimation) {
		AnimatedModel* animModel = static_cast<AnimatedModel*>(animatedModel_);
		const SkinCluster& skinCluster = animModel->GetSkinCluster();

		if (skinCluster.paletteSrvHandle.second.ptr != 0) {
			// パレットSRVをセット
			dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(4, skinCluster.paletteSrvHandle.second);
		}
		else {
			// パレットSRVが無効な場合は警告
			OutputDebugStringA("Object3d::Draw - WARNING: Palette SRV is not set for animated model\n");
		}
	}
	else {
		// 通常のオブジェクトの場合、パレットSRVの設定をスキップ
		// （アニメーションなしのオブジェクトではパレットSRVは不要）
	}

	// マルチマテリアル対応: マテリアルごとに描画
	if (isMultiMaterial) {
		// マルチマテリアルモード
		const std::vector<D3D12_VERTEX_BUFFER_VIEW>& vbViews = model_->GetVertexBufferViews();
		size_t meshIndex = 0;

		for (const auto& matDataPair : modelData.matVertexData) {
			const MaterialVertexData& matVertexData = matDataPair.second;
			size_t materialIndex = matVertexData.materialIndex;

			if (materialIndex >= modelData.materials.size() || meshIndex >= vbViews.size()) {
				OutputDebugStringA(("Object3d::Draw - WARNING: Invalid material index " +
					std::to_string(materialIndex) + " or mesh index " + std::to_string(meshIndex) + "\n").c_str());
				meshIndex++;
				continue;
			}

			// このメッシュ用の頂点バッファを設定（アニメーション対応）
			if (useAnimation) {
				AnimatedModel* animModel = static_cast<AnimatedModel*>(animatedModel_);
				const SkinCluster& skinCluster = animModel->GetSkinCluster();

				// 頂点バッファとインフルエンスバッファを設定
				D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
					vbViews[meshIndex],
					skinCluster.influenceBufferView
				};
				dxCommon_->GetCommandList()->IASetVertexBuffers(0, 2, vbvs);
			}
			else {
				// 通常の頂点バッファのみ
				dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vbViews[meshIndex]);
			}

			// このメッシュのマテリアルに対応するテクスチャをバインド
			const MaterialData& currentMaterial = modelData.materials[materialIndex];
			std::string currentTexturePath = currentMaterial.textureFilePath;

			if (currentTexturePath.empty()) {
				currentTexturePath = TextureManager::GetInstance()->GetDefaultTexturePath();
			}
			else if (!TextureManager::GetInstance()->IsTextureExists(currentTexturePath)) {
				OutputDebugStringA(("Object3d::Draw - WARNING: Texture not found: " + currentTexturePath + "\n").c_str());
				currentTexturePath = TextureManager::GetInstance()->GetDefaultTexturePath();
			}

			// テクスチャをセット
			dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2,
				TextureManager::GetInstance()->GetSrvHandleGPU(currentTexturePath));

			// デバッグログ: どのメッシュとマテリアルを描画しているか
			static int drawCount = 0;
			if (drawCount < 5) {
				OutputDebugStringA(("Object3d::Draw - Drawing mesh " + std::to_string(meshIndex) +
					" with material " + std::to_string(materialIndex) +
					" (texture: " + currentTexturePath + "), " +
					std::to_string(matVertexData.vertices.size()) + " vertices\n").c_str());
			}
			drawCount++;

			// このメッシュの頂点を描画
			uint32_t vertexCount = static_cast<uint32_t>(matVertexData.vertices.size());
			dxCommon_->GetCommandList()->DrawInstanced(vertexCount, 1, 0, 0);

			meshIndex++;
		}
	}
	else {
		// シングルマテリアルモード（従来通り）
		dxCommon_->GetCommandList()->DrawInstanced(model_->GetVertexCount(), 1, 0, 0);
	}
}

void Object3d::Draw(Camera* camera, int* visibleMeshCount, int* culledMeshCount) {
	assert(dxCommon_);
	assert(model_);
	assert(camera);

	// パイプラインの設定
	bool usePBR = false;
	if (model_) {
		const MaterialData& material = model_->GetMaterial();
		usePBR = material.isPBR;
	}

	bool useAnimation = enableAnimation_ && animatedModel_;

	if (usePBR && useAnimation) {
		dxCommon_->GetCommandList()->SetPipelineState(spriteCommon_->GetPBRSkinningPipelineState().Get());
	}
	else if (usePBR) {
		dxCommon_->GetCommandList()->SetPipelineState(spriteCommon_->GetPBRPipelineState().Get());
	}
	else if (useAnimation) {
		dxCommon_->GetCommandList()->SetPipelineState(spriteCommon_->GetSkinningPipelineState().Get());
	}
	else {
		dxCommon_->GetCommandList()->SetPipelineState(spriteCommon_->GetGraphicsPipelineState().Get());
	}

	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const ModelData& modelData = model_->GetModelData();
	bool isMultiMaterial = !modelData.matVertexData.empty();

	// 共通のバッファを設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(5, spotLightResource_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(7, cameraResource_->GetGPUVirtualAddress());

	// 環境マップテクスチャ
	std::string envTexturePath = globalEnvironmentTexturePath_;
	if (envTexturePath.empty() || !TextureManager::GetInstance()->IsTextureExists(envTexturePath)) {
		envTexturePath = "Resources/Models/skybox/warm_restaurant_night_2k.hdr";
		if (!TextureManager::GetInstance()->IsTextureExists(envTexturePath)) {
			TextureManager::GetInstance()->LoadTexture(envTexturePath);
		}
	}
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(6,
		TextureManager::GetInstance()->GetSrvHandleGPU(envTexturePath));

	// パレットSRVの設定（アニメーション用）
	if (useAnimation) {
		AnimatedModel* animModel = static_cast<AnimatedModel*>(animatedModel_);
		const SkinCluster& skinCluster = animModel->GetSkinCluster();
		if (skinCluster.paletteSrvHandle.second.ptr != 0) {
			dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(4, skinCluster.paletteSrvHandle.second);
		}
	}

	// マルチマテリアル対応: メッシュごとにカリング判定
	if (isMultiMaterial) {
		const std::vector<D3D12_VERTEX_BUFFER_VIEW>& vbViews = model_->GetVertexBufferViews();
		size_t meshIndex = 0;

		for (const auto& matDataPair : modelData.matVertexData) {
			const MaterialVertexData& matVertexData = matDataPair.second;
			size_t materialIndex = matVertexData.materialIndex;

			if (materialIndex >= modelData.materials.size() || meshIndex >= vbViews.size()) {
				meshIndex++;
				continue;
			}

			// メッシュごとのバウンディングボックスを計算
			Vector3 meshMin = {FLT_MAX, FLT_MAX, FLT_MAX};
			Vector3 meshMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

			for (const auto& vertex : matVertexData.vertices) {
				// ワールド変換を適用（vertex.positionはVector4）
				Vector4 localPos = vertex.position;
				Matrix4x4 world = transformationMatrixData_->World;

				float x = localPos.x * world.m[0][0] + localPos.y * world.m[1][0] + localPos.z * world.m[2][0] + localPos.w * world.m[3][0];
				float y = localPos.x * world.m[0][1] + localPos.y * world.m[1][1] + localPos.z * world.m[2][1] + localPos.w * world.m[3][1];
				float z = localPos.x * world.m[0][2] + localPos.y * world.m[1][2] + localPos.z * world.m[2][2] + localPos.w * world.m[3][2];
				float w = localPos.x * world.m[0][3] + localPos.y * world.m[1][3] + localPos.z * world.m[2][3] + localPos.w * world.m[3][3];

				Vector3 worldPos = {x / w, y / w, z / w};

				meshMin.x = (std::min)(meshMin.x, worldPos.x);
				meshMin.y = (std::min)(meshMin.y, worldPos.y);
				meshMin.z = (std::min)(meshMin.z, worldPos.z);
				meshMax.x = (std::max)(meshMax.x, worldPos.x);
				meshMax.y = (std::max)(meshMax.y, worldPos.y);
				meshMax.z = (std::max)(meshMax.z, worldPos.z);
			}

			// フラスタムカリング判定
			if (!camera->IsAABBInFrustum(meshMin, meshMax)) {
				if (culledMeshCount) (*culledMeshCount)++;
				meshIndex++;
				continue;
			}

			if (visibleMeshCount) (*visibleMeshCount)++;

			// 頂点バッファを設定
			if (useAnimation) {
				AnimatedModel* animModel = static_cast<AnimatedModel*>(animatedModel_);
				const SkinCluster& skinCluster = animModel->GetSkinCluster();
				D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
					vbViews[meshIndex],
					skinCluster.influenceBufferView
				};
				dxCommon_->GetCommandList()->IASetVertexBuffers(0, 2, vbvs);
			}
			else {
				dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vbViews[meshIndex]);
			}

			// テクスチャをセット
			const MaterialData& currentMaterial = modelData.materials[materialIndex];
			std::string currentTexturePath = currentMaterial.textureFilePath;

			if (currentTexturePath.empty()) {
				currentTexturePath = TextureManager::GetInstance()->GetDefaultTexturePath();
			}
			else if (!TextureManager::GetInstance()->IsTextureExists(currentTexturePath)) {
				currentTexturePath = TextureManager::GetInstance()->GetDefaultTexturePath();
			}

			dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2,
				TextureManager::GetInstance()->GetSrvHandleGPU(currentTexturePath));

			// 描画
			uint32_t vertexCount = static_cast<uint32_t>(matVertexData.vertices.size());
			dxCommon_->GetCommandList()->DrawInstanced(vertexCount, 1, 0, 0);

			meshIndex++;
		}
	}
	else {
		// シングルメッシュの場合は単純なAABBチェック
		Vector3 pos = transform_.translate;
		Vector3 scale = transform_.scale;
		Vector3 min = pos - scale * 0.5f;
		Vector3 max = pos + scale * 0.5f;

		if (camera->IsAABBInFrustum(min, max)) {
			if (visibleMeshCount) (*visibleMeshCount)++;

			if (useAnimation) {
				AnimatedModel* animModel = static_cast<AnimatedModel*>(animatedModel_);
				const SkinCluster& skinCluster = animModel->GetSkinCluster();
				D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
					model_->GetVBView(),
					skinCluster.influenceBufferView
				};
				dxCommon_->GetCommandList()->IASetVertexBuffers(0, 2, vbvs);
			}
			else {
				dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &model_->GetVBView());
			}

			std::string texturePath = model_->GetTextureFilePath();
			if (texturePath.empty()) {
				texturePath = TextureManager::GetInstance()->GetDefaultTexturePath();
			}
			else if (!TextureManager::GetInstance()->IsTextureExists(texturePath)) {
				texturePath = TextureManager::GetInstance()->GetDefaultTexturePath();
			}

			dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2,
				TextureManager::GetInstance()->GetSrvHandleGPU(texturePath));

			dxCommon_->GetCommandList()->DrawInstanced(model_->GetVertexCount(), 1, 0, 0);
		}
		else {
			if (culledMeshCount) (*culledMeshCount)++;
		}
	}
}

void Object3d::SkeletonUpdate(Skeleton& skeleton)
{
	// ← ここでサイズを合わせるのが絶対必要！！
	skeletonPose_.resize(skeleton.joints.size());
	//すべてのjointを更新。親が若いので通常ループで処理可能になっている
	for (Joint& joint : skeleton.joints)
	{
		joint.localMatrix = MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
		if (joint.parent)
		{
			joint.skeletonSpaceMatrix = Multiply(joint.localMatrix, skeleton.joints[*joint.parent].skeletonSpaceMatrix);

		}
		else
		{
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
		skeletonPose_[joint.index] = joint.skeletonSpaceMatrix;
	}

}

void Object3d::ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime)
{
	static int applyCount = 0;
	bool shouldDebug = (applyCount % 300 == 0); // 5秒ごとに出力
	applyCount++;

	for (Joint& joint : skeleton.joints) {
		// 対象のJointのAnimationがあれば、値の適用を行う。
		// 下記のif文はC++17から可能になった初期化付きif文。
		if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
			const NodeAnimation& nodeAnimation = it->second;

			joint.transform.translate = ::CalculateValue(nodeAnimation.translate, animationTime);
			joint.transform.rotate = ::CalculateValue(nodeAnimation.rotate, animationTime);
			joint.transform.scale = ::CalculateValue(nodeAnimation.scale, animationTime);
		}
	}
}

void Object3d::SkinClusterUpdate(SkinCluster& skinCluster, const Skeleton& skeleton)
{
	for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex)
	{
		assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());
		skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix =
			Multiply(skinCluster.inverseBindPoseMatrices[jointIndex], skeleton.joints[jointIndex].skeletonSpaceMatrix);
		skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix =
			Transpose(Inverse(skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix));
	}
}

// CalculateValue関数はAnimationUtilityから使用するため、Object3dクラスからは削除

void Object3d::SetAnimatedModel(class AnimatedModel* animatedModel)
{
	animatedModel_ = animatedModel;
}

void Object3d::SetEnableAnimation(bool enable)
{
	enableAnimation_ = enable;
}

bool Object3d::GetEnableAnimation() const
{
	return enableAnimation_;
}

float Object3d::GetAnimationTime() const
{
	// AnimationPlayerから時刻を取得
	if (animatedModel_) {
		return animatedModel_->GetAnimationPlayer().GetTime();
	}
	return animationTime_;
}

void Object3d::SetAnimationTime(float time)
{
	// AnimationPlayerの時刻を設定
	if (animatedModel_) {
		animatedModel_->GetAnimationPlayer().SetTime(time);
	}
	animationTime_ = time; // 互換性のため残す
}

void Object3d::EnableCollision(bool enabled, const std::string& name) {
    auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
    if (!collisionManager || !model_) return;

    // モデルデータからマルチマテリアルかどうか判定
    const auto& modelData = model_->GetModelData();

    if (!modelData.matVertexData.empty()) {
        // マルチマテリアルの場合はマルチメッシュAABBを登録
        std::vector<Collision::AABB> aabbs = Collision::AABBExtractor::ExtractMultipleAABBsFromAnimatedModel(
            static_cast<AnimatedModel*>(model_));
        collisionManager->RegisterObjectWithMultipleAABBs(this, aabbs, enabled, name);
    } else {
        // シングルマテリアルの場合はシングルAABBを登録
        Collision::AABB aabb = Collision::AABBExtractor::ExtractFromModel(model_);
        collisionManager->RegisterObject(this, aabb, enabled, name);
    }
}

// 静的メンバ変数の定義
std::string Object3d::globalEnvironmentTexturePath_ = "Resources/Models/skybox/warm_restaurant_night_2k.hdr";

void Object3d::SetEnvTex(const std::string& texturePath) {
	globalEnvironmentTexturePath_ = texturePath;

	// テクスチャが存在するか確認し、存在しない場合はロード
	if (!texturePath.empty() && !TextureManager::GetInstance()->IsTextureExists(texturePath)) {
		TextureManager::GetInstance()->LoadTexture(texturePath);
	}

	OutputDebugStringA(("Object3d::SetEnvTex - Global environment texture set to: " + texturePath + "\n").c_str());
}

void Object3d::EnableEnv(bool enable) {
	enableEnvironmentMap_ = enable;
	if (materialData_) {
		materialData_->enableEnvironmentMap = enable ? 1 : 0;
		OutputDebugStringA(("Object3d::EnableEnv - " + std::string(enable ? "Enabled" : "Disabled") + "\n").c_str());
	} else {
		OutputDebugStringA("Object3d::EnableEnv - WARNING: materialData_ is null\n");
	}
}

void Object3d::SetEnvironmentMapIntensity(float intensity) {
	environmentMapIntensity_ = std::clamp(intensity, 0.0f, 1.0f);
	if (materialData_) {
		materialData_->environmentMapIntensity = environmentMapIntensity_;
	}
}
