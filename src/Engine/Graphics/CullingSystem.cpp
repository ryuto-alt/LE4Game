#include "CullingSystem.h"
#include "Camera.h"
#include "Object3d.h"
#include <cmath>

CullingSystem::CullingSystem() {
}

CullingSystem::~CullingSystem() {
}

std::vector<CullingSystem::ObjectInfo> CullingSystem::PerformCulling(
    const std::vector<Object3d*>& objects,
    Camera* camera
) {
    std::vector<ObjectInfo> results;
    results.reserve(objects.size());

    // 統計をリセット
    statistics_ = Statistics();
    statistics_.totalObjects = static_cast<int>(objects.size());

    Vector3 cameraPos = camera->GetTranslate();

    for (auto* obj : objects) {
        if (!obj) continue;

        ObjectInfo info;
        info.object = obj;
        info.position = obj->GetPosition();
        info.boundingRadius = 5.0f; // デフォルト値（後でAABBから計算可能）

        // 距離ベースカリングを実行
        if (lodSettings_.enableDistanceCulling) {
            info.result = CalculateDistanceCulling(info.position, cameraPos, info.distance);

            // 距離カリングされた場合はスキップ
            if (info.result == CullResult::CULLED) {
                statistics_.culledObjects++;
                results.push_back(info);
                continue;
            }
        } else {
            info.distance = CalculateDistance(info.position, cameraPos);
            info.result = CullResult::VISIBLE;
        }

        // LOD判定
        if (lodSettings_.enableLOD && info.result != CullResult::CULLED) {
            if (info.distance <= lodSettings_.highDetailDistance) {
                info.result = CullResult::LOD_HIGH;
                statistics_.lodHighCount++;
            } else if (info.distance <= lodSettings_.mediumDetailDistance) {
                info.result = CullResult::LOD_MEDIUM;
                statistics_.lodMediumCount++;
            } else {
                info.result = CullResult::LOD_LOW;
                statistics_.lodLowCount++;
            }
            statistics_.visibleObjects++;
        } else if (info.result != CullResult::CULLED) {
            statistics_.visibleObjects++;
        }

        results.push_back(info);
    }

    // 統計を更新
    UpdateStatistics(results);

    return results;
}

CullingSystem::CullResult CullingSystem::CalculateDistanceCulling(
    const Vector3& objectPosition,
    const Vector3& cameraPosition,
    float& outDistance
) {
    outDistance = CalculateDistance(objectPosition, cameraPosition);

    if (outDistance > lodSettings_.maxRenderDistance) {
        return CullResult::CULLED;
    }

    return CullResult::VISIBLE;
}

float CullingSystem::CalculateDistance(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void CullingSystem::UpdateStatistics(const std::vector<ObjectInfo>& results) {
    if (results.empty()) return;

    float totalDistance = 0.0f;
    int count = 0;

    for (const auto& info : results) {
        if (info.result != CullResult::CULLED) {
            totalDistance += info.distance;
            count++;
        }
    }

    if (count > 0) {
        statistics_.averageDistance = totalDistance / count;
    }

    if (statistics_.totalObjects > 0) {
        statistics_.cullingRate = (float)statistics_.culledObjects / statistics_.totalObjects * 100.0f;
    }
}