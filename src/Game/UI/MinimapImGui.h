#pragma once
#include <memory>
#include <vector>
#include "../../Engine/Math/Mymath.h"

// Forward declarations
class Player;
class Enemy;
class Orb;

class MinimapImGui {
public:
    MinimapImGui();
    ~MinimapImGui();

    void Initialize();
    void Update(Player* player, Enemy* enemy, const std::vector<std::unique_ptr<Orb>>& orbs);
    void Draw();

    // Orb counter
    int GetTotalOrbs() const { return totalOrbs_; }
    int GetCollectedOrbs() const { return collectedOrbs_; }

private:
    // Minimap settings
    static constexpr float MAP_SIZE = 200.0f;
    static constexpr float MAP_SCALE = 3.0f;  // ワールド座標からミニマップへのスケール
    static constexpr float MAP_PADDING = 10.0f;

    // Orb tracking
    int totalOrbs_ = 0;
    int collectedOrbs_ = 0;

    // Cached positions for rendering
    struct EntityPosition {
        Vector2 mapPos;
        bool active;
    };

    Vector2 playerMapPos_;
    Vector2 enemyMapPos_;
    std::vector<EntityPosition> orbMapPositions_;

    // Helper functions
    Vector2 WorldToMinimap(const Vector3& worldPos, const Vector2& mapCenter) const;
};