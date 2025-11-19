#include "GameClearScene.h"
#include "../../Engine/Resource/ResourcePreloader.h"
#include "SceneManager.h"
#ifdef _DEBUG
#include "imgui.h"
#endif

void GameClearScene::Initialize() {
	if (!dxCommon_ || !srvManager_ || !camera_) {
		OutputDebugStringA("GameClearScene::Initialize - Critical error: Required pointers are null!\n");
		return;
	}

	camera_->SetTranslate({ 0.0f, 0.0f, -10.0f });

	// 黒背景スプライトの初期化
	blackBgSprite_ = std::make_unique<Sprite>();
	blackBgSprite_->Initialize(spriteCommon_, "Resources/textures/white1x1.png");
	blackBgSprite_->SetPosition({ 0.0f, 0.0f });
	blackBgSprite_->SetSize({ 1280.0f, 720.0f });
	blackBgSprite_->setColor({ 0.0f, 0.0f, 0.0f, 1.0f });  // 黒色

	// タイトル画像スプライトの初期化
	titleImageSprite_ = std::make_unique<Sprite>();
	titleImageSprite_->Initialize(spriteCommon_, "Resources/textures/Title/Title_moji.png");
	titleImageSprite_->SetAnchorPoint({ 0.5f, 0.5f });  // 中心基準

	// ロゴ画像スプライトの初期化
	logoImageSprite_ = std::make_unique<Sprite>();
	logoImageSprite_->Initialize(spriteCommon_, "Resources/textures/logo/logo.png");
	logoImageSprite_->SetAnchorPoint({ 0.5f, 0.5f });  // 中心基準

	// エンドロールの初期化
	InitializeCredits();
}

void GameClearScene::InitializeCredits() {
	credits_.clear();

	// タイトル（画像として表示）
	credits_.push_back({ "TITLE_IMAGE", true, true });  // isImage = true
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });

	// ディレクター
	credits_.push_back({ "Director", true, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うの　りゅうと", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });

	// プログラマー
	credits_.push_back({ "Programmer", true, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うの　りゅうと", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });

	// デザイナー
	credits_.push_back({ "Designer", true, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うの　りゅうと", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });

	// サウンド
	credits_.push_back({ "Sound", true, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "AudioStock", false, false });
	credits_.push_back({ "Suno AI", false, false });
	credits_.push_back({ "pixta", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });

	// 使用ライブラリ
	credits_.push_back({ "Special Thanks", true, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うちぼり　ゆうた", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うちぼり　ゆうたのお父さん", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うちぼり　ゆうたのお母さん", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うちぼり　ゆうたのおばあさん", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うちぼり　ゆうたの友達", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うちぼり　ゆうたの猫x2", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うちぼり　ゆうたの従妹x3", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "うちぼり　ゆうたのAirPods", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });

	// 使用エンジン（画像として表示）
	credits_.push_back({ "LOGO_IMAGE", true, true });  // isImage = true
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });

	// 最終クレジット
	credits_.push_back({ "Thank you for playing!", true, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
	credits_.push_back({ "", false, false });
}

void GameClearScene::Update() {
	if (!camera_ || !input_) {
		return;
	}

	camera_->Update();

	float deltaTime = 1.0f / 60.0f;
	time_ += deltaTime;

	// エンドロールのスクロール
	if (!creditsFinished_) {
		scrollOffset_ += scrollSpeed_ * deltaTime;

		// 全てのクレジットがスクロールし終わったかチェック
		float totalHeight = credits_.size() * 40.0f;  // 行間40ピクセル
		if (scrollOffset_ > totalHeight + 720.0f) {  // 画面高さ分余分にスクロール
			creditsFinished_ = true;
			scrollOffset_ = 0.0f;  // リセット
		}
	}

	// 黒背景スプライトの更新
	if (blackBgSprite_) {
		blackBgSprite_->Update();
	}

	// 画像スプライトの位置更新（スクロールに合わせて）
	float startY = 720.0f - scrollOffset_;

	// タイトル画像（インデックス0）
	if (titleImageSprite_) {
		float yPos = startY;  // インデックス0なので (0 * 40.0f) = 0
		titleImageSprite_->SetPosition({ 640.0f, yPos });
		titleImageSprite_->Update();
	}

	// ロゴ画像の位置を計算（配列内でLOGO_IMAGEのインデックスを探す）
	if (logoImageSprite_) {
		for (size_t i = 0; i < credits_.size(); ++i) {
			if (credits_[i].text == "LOGO_IMAGE") {
				float yPos = startY + (i * 40.0f);
				logoImageSprite_->SetPosition({ 640.0f, yPos });
				logoImageSprite_->Update();
				break;
			}
		}
	}

	// SPACEまたはENTERでタイトルに戻る
	if (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)) {
		sceneManager_->ChangeScene("Title");
	}
}

void GameClearScene::Draw() {
	if (spriteCommon_) {
		spriteCommon_->CommonDraw();
	}

	// 黒背景を描画
	if (blackBgSprite_) {
		blackBgSprite_->Draw();
	}

	// タイトル画像を描画（画面内にある場合のみ）
	if (titleImageSprite_) {
		Vector2 pos = titleImageSprite_->GetPosition();
		if (pos.y > -200.0f && pos.y < 920.0f) {  // 画面内判定
			titleImageSprite_->Draw();
		}
	}

	// ロゴ画像を描画（画面内にある場合のみ）
	if (logoImageSprite_) {
		Vector2 pos = logoImageSprite_->GetPosition();
		if (pos.y > -200.0f && pos.y < 920.0f) {  // 画面内判定
			logoImageSprite_->Draw();
		}
	}

	// エンドロール描画
	DrawCredits();
}

void GameClearScene::DrawCredits() {
#ifdef _DEBUG
	// デバッグビルドではImGuiでテキストを表示
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav;

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(1280, 720));

	if (ImGui::Begin("Credits", nullptr, windowFlags)) {
		float startY = 720.0f - scrollOffset_;  // 画面下から開始

		for (size_t i = 0; i < credits_.size(); ++i) {
			const auto& credit = credits_[i];
			float yPos = startY + (i * 40.0f);

			// 画像として表示する行はスキップ（Spriteで描画済み）
			if (credit.isImage) {
				continue;
			}

			// 画面内にある場合のみ描画
			if (yPos > -50.0f && yPos < 770.0f) {
				if (credit.isTitle) {
					// タイトル行
					ImGui::SetWindowFontScale(2.5f);
					float textWidth = ImGui::CalcTextSize(credit.text.c_str()).x;
					ImGui::SetCursorPos(ImVec2(640.0f - textWidth / 2.0f, yPos));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
					ImGui::Text("%s", credit.text.c_str());
					ImGui::PopStyleColor();
					ImGui::SetWindowFontScale(1.0f);
				}
				else if (!credit.text.empty()) {
					// 通常行
					ImGui::SetWindowFontScale(1.6f);
					float textWidth = ImGui::CalcTextSize(credit.text.c_str()).x;
					ImGui::SetCursorPos(ImVec2(640.0f - textWidth / 2.0f, yPos));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
					ImGui::Text("%s", credit.text.c_str());
					ImGui::PopStyleColor();
					ImGui::SetWindowFontScale(1.0f);
				}
			}
		}
	}
	ImGui::End();
#endif
	// リリースビルドでは黒背景のみ表示（テキストはなし）
	// TODO: リリース用にテキスト画像を用意する必要があります
}

void GameClearScene::Finalize() {
	// エンドロール用のクリーンアップ
	credits_.clear();
	blackBgSprite_.reset();
	titleImageSprite_.reset();
	logoImageSprite_.reset();
}
