#include "Frustum.h"
#include <cmath>

Frustum::Frustum() {
    // 平面の初期化
    for (int i = 0; i < 6; ++i) {
        planes_[i] = Plane();
    }
}

Frustum::~Frustum() {
}

void Frustum::BuildFromViewProjection(const Matrix4x4& vp) {
    // ビュープロジェクション行列から6つの平面を抽出
    // この方法はGribb & Hartmannアルゴリズムを使用
    // http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf

    const float* m = &vp.m[0][0];

    // 左平面: 第4列 + 第1列
    planes_[PLANE_LEFT].normal.x = m[3] + m[0];
    planes_[PLANE_LEFT].normal.y = m[7] + m[4];
    planes_[PLANE_LEFT].normal.z = m[11] + m[8];
    planes_[PLANE_LEFT].distance = m[15] + m[12];
    NormalizePlane(planes_[PLANE_LEFT]);

    // 右平面: 第4列 - 第1列
    planes_[PLANE_RIGHT].normal.x = m[3] - m[0];
    planes_[PLANE_RIGHT].normal.y = m[7] - m[4];
    planes_[PLANE_RIGHT].normal.z = m[11] - m[8];
    planes_[PLANE_RIGHT].distance = m[15] - m[12];
    NormalizePlane(planes_[PLANE_RIGHT]);

    // 下平面: 第4列 + 第2列
    planes_[PLANE_BOTTOM].normal.x = m[3] + m[1];
    planes_[PLANE_BOTTOM].normal.y = m[7] + m[5];
    planes_[PLANE_BOTTOM].normal.z = m[11] + m[9];
    planes_[PLANE_BOTTOM].distance = m[15] + m[13];
    NormalizePlane(planes_[PLANE_BOTTOM]);

    // 上平面: 第4列 - 第2列
    planes_[PLANE_TOP].normal.x = m[3] - m[1];
    planes_[PLANE_TOP].normal.y = m[7] - m[5];
    planes_[PLANE_TOP].normal.z = m[11] - m[9];
    planes_[PLANE_TOP].distance = m[15] - m[13];
    NormalizePlane(planes_[PLANE_TOP]);

    // 近平面: 第4列 + 第3列
    planes_[PLANE_NEAR].normal.x = m[3] + m[2];
    planes_[PLANE_NEAR].normal.y = m[7] + m[6];
    planes_[PLANE_NEAR].normal.z = m[11] + m[10];
    planes_[PLANE_NEAR].distance = m[15] + m[14];
    NormalizePlane(planes_[PLANE_NEAR]);

    // 遠平面: 第4列 - 第3列
    planes_[PLANE_FAR].normal.x = m[3] - m[2];
    planes_[PLANE_FAR].normal.y = m[7] - m[6];
    planes_[PLANE_FAR].normal.z = m[11] - m[10];
    planes_[PLANE_FAR].distance = m[15] - m[14];
    NormalizePlane(planes_[PLANE_FAR]);
}

void Frustum::NormalizePlane(Plane& plane) {
    // 法線ベクトルの長さを計算
    float length = sqrtf(
        plane.normal.x * plane.normal.x +
        plane.normal.y * plane.normal.y +
        plane.normal.z * plane.normal.z
    );

    // 0除算を防ぐ
    if (length > 0.0001f) {
        float invLength = 1.0f / length;
        plane.normal.x *= invLength;
        plane.normal.y *= invLength;
        plane.normal.z *= invLength;
        plane.distance *= invLength;
    }
}

bool Frustum::IsPointInside(const Vector3& point) const {
    // 全ての平面に対して点が内側にあるかチェック
    for (int i = 0; i < 6; ++i) {
        const Plane& plane = planes_[i];
        float distance =
            plane.normal.x * point.x +
            plane.normal.y * point.y +
            plane.normal.z * point.z +
            plane.distance;

        // 点が平面の外側にある場合
        if (distance < 0.0f) {
            return false;
        }
    }

    return true;
}

bool Frustum::IsAABBInside(const Vector3& min, const Vector3& max) const {
    // AABBの8つの頂点を計算して、少なくとも1つが内側にあるかチェック
    // 最適化版: 各平面に対してAABBの最も遠い頂点（p-vertex）をテスト

    for (int i = 0; i < 6; ++i) {
        const Plane& plane = planes_[i];

        // AABBの最も遠い頂点（positive vertex）を計算
        Vector3 pVertex;
        pVertex.x = (plane.normal.x >= 0.0f) ? max.x : min.x;
        pVertex.y = (plane.normal.y >= 0.0f) ? max.y : min.y;
        pVertex.z = (plane.normal.z >= 0.0f) ? max.z : min.z;

        // p-vertexが平面の外側にある場合、AABBは完全に外側
        float distance =
            plane.normal.x * pVertex.x +
            plane.normal.y * pVertex.y +
            plane.normal.z * pVertex.z +
            plane.distance;

        if (distance < 0.0f) {
            return false;
        }
    }

    return true;
}

bool Frustum::IsSphereInside(const Vector3& center, float radius) const {
    // 全ての平面に対して球が内側にあるかチェック
    for (int i = 0; i < 6; ++i) {
        const Plane& plane = planes_[i];
        float distance =
            plane.normal.x * center.x +
            plane.normal.y * center.y +
            plane.normal.z * center.z +
            plane.distance;

        // 球の中心が平面から半径以上離れている場合、完全に外側
        if (distance < -radius) {
            return false;
        }
    }

    return true;
}
