#pragma once
#include "Mymath.h"

// 平面構造体
struct Plane {
    Vector3 normal;  // 平面の法線ベクトル
    float distance;  // 原点からの距離

    Plane() : normal({0.0f, 0.0f, 0.0f}), distance(0.0f) {}
    Plane(const Vector3& n, float d) : normal(n), distance(d) {}
};

// フラスタム（視錐台）クラス
class Frustum {
public:
    // 6つの平面のインデックス
    enum PlaneIndex {
        PLANE_LEFT = 0,
        PLANE_RIGHT = 1,
        PLANE_BOTTOM = 2,
        PLANE_TOP = 3,
        PLANE_NEAR = 4,
        PLANE_FAR = 5
    };

    Frustum();
    ~Frustum();

    // ビュープロジェクション行列からフラスタムを構築
    void BuildFromViewProjection(const Matrix4x4& viewProjection);

    // 点がフラスタム内にあるかテスト
    bool IsPointInside(const Vector3& point) const;

    // AABBがフラスタムと交差するかテスト
    bool IsAABBInside(const Vector3& min, const Vector3& max) const;

    // 球がフラスタムと交差するかテスト
    bool IsSphereInside(const Vector3& center, float radius) const;

    // 平面の取得
    const Plane& GetPlane(PlaneIndex index) const { return planes_[index]; }

private:
    // 平面の正規化
    void NormalizePlane(Plane& plane);

    // 6つの平面（左、右、下、上、近、遠）
    Plane planes_[6];
};
