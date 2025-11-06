#include "TextRenderer.h"
#include "../../Engine/Graphics/DirectXCommon.h"
#include "../../Engine/Graphics/SRVManager.h"
#include "../../Engine/Graphics/Sprite.h"
#include "../../Engine/Graphics/SpriteCommon.h"
#include "../../Engine/Graphics/TextureManager.h"
#include <format>
#include <vector>

TextRenderer::TextRenderer() {
}

TextRenderer::~TextRenderer() {
}

void TextRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_);

    InitializeCharacterMap();

    // Pre-allocate text sprites
    textSpritesPool_.reserve(MAX_TEXT_SPRITES);
    for (int i = 0; i < MAX_TEXT_SPRITES; ++i) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(spriteCommon_.get(), "Resources/Models/Enemy/Enemy_Run/default_baseColor.png");
        textSpritesPool_.push_back(std::move(sprite));
    }
}

void TextRenderer::LoadFont(const std::string& fontPath) {
    // For now, we'll use simple colored rectangles for numbers
    // In a real implementation, you would load the TTF font and render to texture
    fontTextureHandle_ = 0;  // Placeholder
}

void TextRenderer::InitializeCharacterMap() {
    // Initialize character mapping for digital display
    // This would map characters to their positions in the font texture
}

void TextRenderer::RenderText(const std::string& text, const Vector2& position, float scale, const Vector4& color) {
    if (!spriteCommon_) return;

    float currentX = position.x;
    const float charWidth = 20.0f * scale;
    const float charHeight = 30.0f * scale;

    ResetSpritePool();

    for (char c : text) {
        Sprite* sprite = GetNextSprite();
        if (!sprite) break;

        // Configure sprite for this character
        if (c >= '0' && c <= '9') {
            sprite->SetPosition({ currentX, position.y });
            sprite->SetSize({ 16.0f * scale, 24.0f * scale });
            currentX += 16.0f * scale;
        } else if (c == '/') {
            sprite->SetPosition({ currentX, position.y });
            sprite->SetSize({ 8.0f * scale, 24.0f * scale });
            currentX += 8.0f * scale;
        } else if (c == ':') {
            sprite->SetPosition({ currentX, position.y });
            sprite->SetSize({ 8.0f * scale, 24.0f * scale });
            currentX += 8.0f * scale;
        } else if (c >= 'A' && c <= 'Z') {
            sprite->SetPosition({ currentX, position.y });
            sprite->SetSize({ 16.0f * scale, 24.0f * scale });
            currentX += 16.0f * scale;
        } else {
            // Space or other characters
            currentX += 10.0f * scale;
            continue;
        }

        sprite->Update();
        sprite->Draw();
    }
}

void TextRenderer::RenderOrbCounter(int collected, int total, const Vector2& position) {
    std::string counterText = std::format("{:02d}/{:02d}", collected, total);

    // Render "ORBS:" label
    const float labelScale = 1.2f;
    Vector4 labelColor = { 0.8f, 0.8f, 0.8f, 1.0f };
    RenderText("ORBS:", position, labelScale, labelColor);

    // Render the counter
    Vector2 counterPos = { position.x + 100, position.y };
    Vector4 counterColor;

    float percentage = total > 0 ? static_cast<float>(collected) / total : 0.0f;
    if (percentage >= 1.0f) {
        counterColor = { 0.0f, 1.0f, 0.0f, 1.0f };  // Green when complete
    } else if (percentage >= 0.5f) {
        counterColor = { 1.0f, 1.0f, 0.0f, 1.0f };  // Yellow when halfway
    } else {
        counterColor = { 1.0f, 0.5f, 0.0f, 1.0f };  // Orange default
    }

    RenderText(counterText, counterPos, labelScale, counterColor);
}

void TextRenderer::RenderTimer(float time, const Vector2& position) {
    int minutes = static_cast<int>(time) / 60;
    int seconds = static_cast<int>(time) % 60;
    int milliseconds = static_cast<int>((time - static_cast<int>(time)) * 100);

    std::string timeText = std::format("{:02d}:{:02d}.{:02d}", minutes, seconds, milliseconds);
    RenderText(timeText, position, 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
}

Sprite* TextRenderer::GetNextSprite() {
    if (currentSpriteIndex_ < textSpritesPool_.size()) {
        return textSpritesPool_[currentSpriteIndex_++].get();
    }
    return nullptr;
}

void TextRenderer::ResetSpritePool() {
    currentSpriteIndex_ = 0;
}

TextRenderer::CharacterInfo TextRenderer::GetCharacterInfo(char c) const {
    CharacterInfo info;
    // Default values - would be loaded from font data
    info.u = 0.0f;
    info.v = 0.0f;
    info.width = 16.0f;
    info.height = 24.0f;
    return info;
}