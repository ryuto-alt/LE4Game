#pragma once
#include <memory>
#include <vector>
#include <string>
#include "../../Engine/Math/Mymath.h"

// Forward declarations
class DirectXCommon;
class SrvManager;
class Camera;
class Player;
class Enemy;
class Orb;
class Sprite;
class SpriteCommon;
class TextRenderer;

class Minimap {
public:
    Minimap();
    ~Minimap();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update(Player* player, Enemy* enemy, const std::vector<std::unique_ptr<Orb>>& orbs);
    void Draw();

    // Orb counter
    int GetTotalOrbs() const { return totalOrbs_; }
    int GetCollectedOrbs() const { return collectedOrbs_; }

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // Minimap settings
    static constexpr float MAP_SIZE = 200.0f;  // ミニマップのサイズ
    static constexpr float MAP_SCALE = 0.02f;  // ワールド座標からミニマップへのスケール
    static constexpr float MAP_X = 10.0f;      // 左からの距離
    static constexpr float MAP_Y_OFFSET = 10.0f;  // 下からの距離

    // Sprites
    std::unique_ptr<SpriteCommon> spriteCommon_;
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> playerSprite_;
    std::unique_ptr<Sprite> enemySprite_;
    std::unique_ptr<Sprite> borderSprite_;

    // Pre-allocated orb sprites pool (max 70 orbs)
    static constexpr int MAX_ORBS = 70;
    std::vector<std::unique_ptr<Sprite>> orbSpritesPool_;
    std::vector<int> activeOrbIndices_;  // Indices of active orbs

    // Text rendering for orb counter
    std::unique_ptr<TextRenderer> textRenderer_;

    // Orb tracking
    int totalOrbs_ = 0;
    int collectedOrbs_ = 0;

    // Helper functions
    Vector2 WorldToMinimap(const Vector3& worldPos) const;
    void CreateTextures();
    void UpdateOrbCounter();
};