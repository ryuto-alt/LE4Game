#pragma once
#include "Mymath.h"
#include <vector>
#include <memory>

class Camera;
class Object3d;

// カリングシステムクラス
class CullingSystem {
public:
    // LOD (Level of Detail) 設定
    struct LODSettings {
        float maxRenderDistance = 300.0f;      // 最大描画距離
        float highDetailDistance = 50.0f;      // 高品質描画距離
        float mediumDetailDistance = 150.0f;   // 中品質描画距離
        bool enableDistanceCulling = true;     // 距離カリングの有効/無効
        bool enableLOD = true;                 // LODシステムの有効/無効
    };

    // カリング結果
    enum class CullResult {
        VISIBLE,            // 完全に描画
        CULLED,            // カリングされた
        LOD_HIGH,          // 高品質描画
        LOD_MEDIUM,        // 中品質描画
        LOD_LOW            // 低品質描画
    };

    // オブジェクト情報
    struct ObjectInfo {
        Object3d* object;
        Vector3 position;
        float boundingRadius;
        CullResult result;
        float distance;
    };

public:
    CullingSystem();
    ~CullingSystem();

    // LOD設定の取得・設定
    void SetLODSettings(const LODSettings& settings) { lodSettings_ = settings; }
    const LODSettings& GetLODSettings() const { return lodSettings_; }

    // カリング実行
    std::vector<ObjectInfo> PerformCulling(
        const std::vector<Object3d*>& objects,
        Camera* camera
    );

    // 距離ベースカリング
    CullResult CalculateDistanceCulling(
        const Vector3& objectPosition,
        const Vector3& cameraPosition,
        float& outDistance
    );

    // 統計情報の取得
    struct Statistics {
        int totalObjects = 0;
        int visibleObjects = 0;
        int culledObjects = 0;
        int lodHighCount = 0;
        int lodMediumCount = 0;
        int lodLowCount = 0;
        float averageDistance = 0.0f;
        float cullingRate = 0.0f;
    };

    const Statistics& GetStatistics() const { return statistics_; }

private:
    LODSettings lodSettings_;
    Statistics statistics_;

    // ヘルパー関数
    float CalculateDistance(const Vector3& a, const Vector3& b);
    void UpdateStatistics(const std::vector<ObjectInfo>& results);
};