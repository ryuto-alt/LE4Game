#include "Minimap.h"
#include "TextRenderer.h"
#include "../../Engine/Graphics/DirectXCommon.h"
#include "../../Engine/Graphics/SRVManager.h"
#include "../../Engine/Graphics/Sprite.h"
#include "../../Engine/Graphics/SpriteCommon.h"
#include "../../Engine/Graphics/TextureManager.h"
#include "../GameObject/Player.h"
#include "../GameObject/Enemy.h"
#include "../GameObject/Orb.h"
#include "../../Engine/Utility/WinApp.h"
#include <format>

Minimap::Minimap() {
}

Minimap::~Minimap() {
}

void Minimap::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    OutputDebugStringA("Minimap::Initialize - Starting initialization\n");

    // Initialize sprite common
    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_);

    // Get window dimensions for positioning
    float windowHeight = static_cast<float>(WinApp::kClientHeight);
    float mapY = windowHeight - MAP_SIZE - MAP_Y_OFFSET;

    // Create minimap background (dark semi-transparent)
    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(spriteCommon_.get(), "Resources/Models/Enemy/Enemy_Run/default_baseColor.png");
    backgroundSprite_->SetPosition({ MAP_X, mapY });
    backgroundSprite_->SetSize({ MAP_SIZE, MAP_SIZE });

    // Create minimap border
    borderSprite_ = std::make_unique<Sprite>();
    borderSprite_->Initialize(spriteCommon_.get(), "Resources/Models/Enemy/Enemy_Run/default_baseColor.png");
    borderSprite_->SetPosition({ MAP_X - 2, mapY - 2 });
    borderSprite_->SetSize({ MAP_SIZE + 4, MAP_SIZE + 4 });

    // Create player icon (green dot)
    playerSprite_ = std::make_unique<Sprite>();
    playerSprite_->Initialize(spriteCommon_.get(), "Resources/Models/Enemy/Enemy_Run/default_baseColor.png");
    playerSprite_->SetSize({ 8.0f, 8.0f });

    // Create enemy icon (red dot)
    enemySprite_ = std::make_unique<Sprite>();
    enemySprite_->Initialize(spriteCommon_.get(), "Resources/Models/Enemy/Enemy_Run/default_baseColor.png");
    enemySprite_->SetSize({ 8.0f, 8.0f });

    // Initialize text renderer for orb counter
    textRenderer_ = std::make_unique<TextRenderer>();
    textRenderer_->Initialize(dxCommon, srvManager);
    textRenderer_->LoadFont("Resources/fonts/DigitalNumbers-Regular.ttf");

    // Pre-allocate orb sprites to avoid dynamic allocation during gameplay
    orbSpritesPool_.reserve(MAX_ORBS);
    for (int i = 0; i < MAX_ORBS; ++i) {
        auto orbSprite = std::make_unique<Sprite>();
        orbSprite->Initialize(spriteCommon_.get(), "Resources/Models/Enemy/Enemy_Run/default_baseColor.png");
        orbSprite->SetSize({ 4.0f, 4.0f });
        orbSpritesPool_.push_back(std::move(orbSprite));
    }

    OutputDebugStringA("Minimap::Initialize - Completed successfully\n");
}

void Minimap::Update(Player* player, Enemy* enemy, const std::vector<std::unique_ptr<Orb>>& orbs) {
    if (!player) return;

    // Get window dimensions
    float windowHeight = static_cast<float>(WinApp::kClientHeight);
    float mapY = windowHeight - MAP_SIZE - MAP_Y_OFFSET;

    // Update player position on minimap
    Vector3 playerWorldPos = player->GetPosition();
    Vector2 playerMapPos = WorldToMinimap(playerWorldPos);
    playerMapPos.x += MAP_X + MAP_SIZE / 2.0f;
    playerMapPos.y += mapY + MAP_SIZE / 2.0f;
    playerSprite_->SetPosition({ playerMapPos.x - 4, playerMapPos.y - 4 });

    // Update enemy position on minimap
    if (enemy) {
        Vector3 enemyWorldPos = enemy->GetPosition();
        Vector2 enemyMapPos = WorldToMinimap(enemyWorldPos);
        enemyMapPos.x += MAP_X + MAP_SIZE / 2.0f;
        enemyMapPos.y += mapY + MAP_SIZE / 2.0f;
        enemySprite_->SetPosition({ enemyMapPos.x - 4, enemyMapPos.y - 4 });
    }

    // Update orb positions and count
    activeOrbIndices_.clear();
    totalOrbs_ = 0;
    collectedOrbs_ = 0;

    int spriteIndex = 0;
    for (const auto& orb : orbs) {
        if (!orb) continue;

        totalOrbs_++;

        if (orb->IsCollected()) {
            collectedOrbs_++;
            continue;  // Don't show collected orbs on minimap
        }

        // Use pre-allocated sprite from pool
        if (spriteIndex < MAX_ORBS && spriteIndex < orbSpritesPool_.size()) {
            Vector3 orbWorldPos = orb->GetPosition();
            Vector2 orbMapPos = WorldToMinimap(orbWorldPos);
            orbMapPos.x += MAP_X + MAP_SIZE / 2.0f;
            orbMapPos.y += mapY + MAP_SIZE / 2.0f;

            orbSpritesPool_[spriteIndex]->SetPosition({ orbMapPos.x - 2, orbMapPos.y - 2 });
            activeOrbIndices_.push_back(spriteIndex);
            spriteIndex++;
        }
    }

    UpdateOrbCounter();
}

void Minimap::Draw() {
    if (!spriteCommon_ || !backgroundSprite_) {
        OutputDebugStringA("Minimap::Draw - Missing required objects!\n");
        return;
    }

    OutputDebugStringA("Minimap::Draw - Starting draw\n");

    // Start sprite drawing - this sets up the rendering pipeline
    spriteCommon_->CommonDraw();

    // Draw minimap layers in order (back to front)
    if (borderSprite_) {
        borderSprite_->Update();
        borderSprite_->Draw();
    }

    if (backgroundSprite_) {
        backgroundSprite_->Update();
        backgroundSprite_->Draw();
    }

    // Draw only active orbs using indices
    for (int index : activeOrbIndices_) {
        if (index < orbSpritesPool_.size() && orbSpritesPool_[index]) {
            orbSpritesPool_[index]->Update();
            orbSpritesPool_[index]->Draw();
        }
    }

    // Draw enemy
    if (enemySprite_) {
        enemySprite_->Update();
        enemySprite_->Draw();
    }

    // Draw player (on top)
    if (playerSprite_) {
        playerSprite_->Update();
        playerSprite_->Draw();
    }

    // Draw orb counter text
    if (textRenderer_) {
        float windowHeight = static_cast<float>(WinApp::kClientHeight);
        float mapY = windowHeight - MAP_SIZE - MAP_Y_OFFSET;
        Vector2 counterPos = { MAP_X, mapY - 40 };
        textRenderer_->RenderOrbCounter(collectedOrbs_, totalOrbs_, counterPos);
    }
}

Vector2 Minimap::WorldToMinimap(const Vector3& worldPos) const {
    // Convert world coordinates to minimap coordinates
    // Center the minimap at origin (0, 0) in world space
    Vector2 mapPos;
    mapPos.x = worldPos.x * MAP_SCALE;
    mapPos.y = -worldPos.z * MAP_SCALE;  // Z becomes Y on 2D map, inverted

    // Clamp to minimap bounds
    float halfSize = MAP_SIZE / 2.0f * 0.9f;  // Keep icons within bounds
    if (mapPos.x < -halfSize) mapPos.x = -halfSize;
    if (mapPos.x > halfSize) mapPos.x = halfSize;
    if (mapPos.y < -halfSize) mapPos.y = -halfSize;
    if (mapPos.y > halfSize) mapPos.y = halfSize;

    return mapPos;
}

void Minimap::UpdateOrbCounter() {
    // Orb counter is now handled by TextRenderer in Draw()
}