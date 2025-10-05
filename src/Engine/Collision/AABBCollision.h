#pragma once
#include "Mymath.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>

// 前方宣言
class Object3d;
class Model;
class AnimatedModel;

namespace Collision {

    // AABB構造体
    struct AABB {
        Vector3 min;  // 最小点
        Vector3 max;  // 最大点

        AABB();
        AABB(const Vector3& minPoint, const Vector3& maxPoint);

        // 中心座標を取得
        Vector3 GetCenter() const;

        // サイズ(各軸の長さ)を取得
        Vector3 GetSize() const;

        // ハーフサイズ(各軸の半分の長さ)を取得
        Vector3 GetHalfSize() const;
    };

    // トランスフォーム適用後のAABBを計算
    AABB TransformAABB(const AABB& aabb, const Vector3& position, const Vector3& scale);

    // AABB同士の衝突判定
    bool CheckAABBCollision(const AABB& a, const AABB& b);

    // GLTFモデルからAABBを抽出するヘルパー
    class AABBExtractor {
    public:
        // GLTFモデルのアクセサーからAABBを抽出
        static AABB ExtractFromGLTF(const void* gltfModel, int meshIndex = 0, int primitiveIndex = 0);

        // ModelクラスからAABBを抽出
        static AABB ExtractFromModel(const Model* model);

        // AnimatedModelクラスからAABBを抽出
        static AABB ExtractFromAnimatedModel(const AnimatedModel* model);

        // AnimatedModelから複数メッシュのAABBを抽出
        static std::vector<AABB> ExtractMultipleAABBsFromAnimatedModel(const AnimatedModel* model);
    };

    // コリジョン対象オブジェクト
    class CollisionObject3D {
    public:
        CollisionObject3D(Object3d* object, const AABB& localAABB, const std::string& name = "");
        ~CollisionObject3D() = default;

        // 更新(ワールドAABBを再計算)
        void Update();

        // ゲッター
        Object3d* GetObject() const { return object_; }
        const AABB& GetLocalAABB() const { return localAABB_; }
        const AABB& GetWorldAABB() const { return worldAABB_; }
        bool IsEnabled() const { return enabled_; }
        const std::string& GetName() const { return name_; }

        // セッター
        void SetEnabled(bool enabled) { enabled_ = enabled; }
        void SetLocalAABB(const AABB& aabb) { localAABB_ = aabb; }
        void SetName(const std::string& name) { name_ = name; }

    private:
        Object3d* object_;      // 参照するObject3d
        AABB localAABB_;        // ローカル座標系でのAABB
        AABB worldAABB_;        // ワールド座標系でのAABB
        bool enabled_;          // 有効フラグ
        std::string name_;      // デバッグ用名前
    };

    // AABBコリジョンマネージャー
    class AABBCollisionManager {
    public:
        static AABBCollisionManager* GetInstance();
        static void Create();
        static void Destroy();

        // コリジョンオブジェクトの登録
        void RegisterObject(Object3d* object, const AABB& localAABB, bool enabled = true, const std::string& name = "");

        // 複数AABBの登録（マルチメッシュ対応）
        void RegisterObjectWithMultipleAABBs(Object3d* object, const std::vector<AABB>& localAABBs, bool enabled = true, const std::string& name = "");

        // コリジョンオブジェクトの削除
        void UnregisterObject(Object3d* object);

        // 全てクリア
        void Clear();

        // 更新(全オブジェクトのワールドAABB更新と衝突判定)
        void Update();

        // 衝突コールバック設定
        using CollisionCallback = std::function<void(Object3d*, Object3d*)>;
        void SetCollisionCallback(CollisionCallback callback) { collisionCallback_ = callback; }

        // デバッグ描画用にAABBリストを取得
        const std::vector<std::shared_ptr<CollisionObject3D>>& GetCollisionObjects() const {
            return collisionObjects_;
        }

        // ImGui デバッグUI表示
        void DrawImGui();

        // CollisionObjectの検索（衝突応答用）
        std::shared_ptr<CollisionObject3D> FindCollisionObject(Object3d* object);

    private:
        AABBCollisionManager() = default;
        ~AABBCollisionManager() = default;
        AABBCollisionManager(const AABBCollisionManager&) = delete;
        AABBCollisionManager& operator=(const AABBCollisionManager&) = delete;

        static AABBCollisionManager* instance_;
        std::vector<std::shared_ptr<CollisionObject3D>> collisionObjects_;
        CollisionCallback collisionCallback_;

        // 衝突検出用
        struct CollisionPair {
            std::shared_ptr<CollisionObject3D> objA;
            std::shared_ptr<CollisionObject3D> objB;
        };;
        std::vector<CollisionPair> currentCollisions_;
    };

} // namespace Collision
