#pragma once
#include "Mymath.h"

namespace Collision {
    // 球
    struct Sphere {
        Vector3 center; // 中心座標
        float radius;   // 半径

        // コンストラクタ
        Sphere() : center({ 0.0f, 0.0f, 0.0f }), radius(1.0f) {}
        Sphere(const Vector3& center, float radius) : center(center), radius(radius) {}
    };

    // 線分
    struct Segment {
        Vector3 start;  // 始点
        Vector3 end;    // 終点

        // コンストラクタ
        Segment() : start({ 0.0f, 0.0f, 0.0f }), end({ 0.0f, 0.0f, 0.0f }) {}
        Segment(const Vector3& start, const Vector3& end) : start(start), end(end) {}
    };

    // カプセル
    struct Capsule {
        Segment segment; // 中心の線分
        float radius;    // 半径

        // コンストラクタ
        Capsule() : segment(), radius(1.0f) {}
        Capsule(const Segment& segment, float radius) : segment(segment), radius(radius) {}
        Capsule(const Vector3& start, const Vector3& end, float radius)
            : segment(start, end), radius(radius) {
        }
    };

    // OBB（有向境界ボックス）- 基本実装のみ
    struct OBB {
        Vector3 center;     // 中心点
        Vector3 size;       // 各軸方向の長さの半分
        Matrix4x4 rotation; // 回転行列

        // コンストラクタ
        OBB() : center({ 0.0f, 0.0f, 0.0f }), size({ 1.0f, 1.0f, 1.0f }) {
            rotation = {}; // 単位行列に初期化
        }
    };
} // namespace Collision