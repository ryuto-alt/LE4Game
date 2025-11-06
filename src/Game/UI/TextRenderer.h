#pragma once
#include <string>
#include <memory>
#include "../../Engine/Math/Mymath.h"

// Forward declarations
class DirectXCommon;
class SrvManager;
class Sprite;
class SpriteCommon;

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void LoadFont(const std::string& fontPath);
    void RenderText(const std::string& text, const Vector2& position, float scale = 1.0f, const Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f});

    // Specific render functions for game UI
    void RenderOrbCounter(int collected, int total, const Vector2& position);
    void RenderTimer(float time, const Vector2& position);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::unique_ptr<SpriteCommon> spriteCommon_;

    // Font texture handle
    uint32_t fontTextureHandle_ = 0;

    // Pre-allocated sprite pool for text rendering
    static constexpr int MAX_TEXT_SPRITES = 100;
    std::vector<std::unique_ptr<Sprite>> textSpritesPool_;
    int currentSpriteIndex_ = 0;

    // Character mapping for digital numbers font
    struct CharacterInfo {
        float u, v;      // UV coordinates in texture
        float width, height;  // Character dimensions
    };

    void InitializeCharacterMap();
    CharacterInfo GetCharacterInfo(char c) const;

    // Helper function to get next sprite from pool
    Sprite* GetNextSprite();
    void ResetSpritePool();
};