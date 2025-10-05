#include "AABBCollision.h"
#include "Object3d.h"
#include "Model.h"
#include "AnimatedModel.h"
#include "../externals/tinygltf/tiny_gltf.h"
#ifdef _DEBUG
#include "imgui.h"
#endif
#include <algorithm>
#include <cassert>

// Windowsのmin/maxマクロを無効化
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#ifdef GetObject
#undef GetObject
#endif

namespace Collision {

    // 静的メンバの初期化
    AABBCollisionManager* AABBCollisionManager::instance_ = nullptr;

    // AABBの実装
    AABB::AABB() {
        min = Vector3{ 0.0f, 0.0f, 0.0f };
        max = Vector3{ 0.0f, 0.0f, 0.0f };
    }

    AABB::AABB(const Vector3& minPoint, const Vector3& maxPoint) {
        min = minPoint;
        max = maxPoint;
    }

    Vector3 AABB::GetCenter() const {
        Vector3 result;
        result.x = (min.x + max.x) * 0.5f;
        result.y = (min.y + max.y) * 0.5f;
        result.z = (min.z + max.z) * 0.5f;
        return result;
    }

    Vector3 AABB::GetSize() const {
        Vector3 result;
        result.x = max.x - min.x;
        result.y = max.y - min.y;
        result.z = max.z - min.z;
        return result;
    }

    Vector3 AABB::GetHalfSize() const {
        Vector3 result;
        result.x = (max.x - min.x) * 0.5f;
        result.y = (max.y - min.y) * 0.5f;
        result.z = (max.z - min.z) * 0.5f;
        return result;
    }

    // トランスフォーム適用後のAABBを計算
    AABB TransformAABB(const AABB& aabb, const Vector3& position, const Vector3& scale) {
        // スケールを適用
        Vector3 scaledMin = {
            aabb.min.x * scale.x,
            aabb.min.y * scale.y,
            aabb.min.z * scale.z
        };
        Vector3 scaledMax = {
            aabb.max.x * scale.x,
            aabb.max.y * scale.y,
            aabb.max.z * scale.z
        };

        // スケールが負の場合、min/maxが入れ替わる
        if (scale.x < 0) std::swap(scaledMin.x, scaledMax.x);
        if (scale.y < 0) std::swap(scaledMin.y, scaledMax.y);
        if (scale.z < 0) std::swap(scaledMin.z, scaledMax.z);

        // 位置を適用
        AABB result;
        result.min = {
            scaledMin.x + position.x,
            scaledMin.y + position.y,
            scaledMin.z + position.z
        };
        result.max = {
            scaledMax.x + position.x,
            scaledMax.y + position.y,
            scaledMax.z + position.z
        };

        return result;
    }

    // AABB同士の衝突判定
    bool CheckAABBCollision(const AABB& a, const AABB& b) {
        return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
               (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
               (a.min.z <= b.max.z && a.max.z >= b.min.z);
    }

    // GLTFモデルのアクセサーからAABBを抽出
    AABB AABBExtractor::ExtractFromGLTF(const void* gltfModelPtr, int meshIndex, int primitiveIndex) {
        const tinygltf::Model* gltfModel = static_cast<const tinygltf::Model*>(gltfModelPtr);

        if (!gltfModel || meshIndex >= gltfModel->meshes.size()) {
            return AABB();  // デフォルトAABB
        }

        const auto& mesh = gltfModel->meshes[meshIndex];
        if (primitiveIndex >= mesh.primitives.size()) {
            return AABB();
        }

        const auto& primitive = mesh.primitives[primitiveIndex];
        auto posIt = primitive.attributes.find("POSITION");
        if (posIt == primitive.attributes.end()) {
            return AABB();
        }

        const auto& accessor = gltfModel->accessors[posIt->second];

        // min/maxがない場合はデフォルト
        if (accessor.minValues.size() < 3 || accessor.maxValues.size() < 3) {
            return AABB();
        }

        AABB result;
        result.min = {
            static_cast<float>(accessor.minValues[0]),
            static_cast<float>(accessor.minValues[1]),
            static_cast<float>(accessor.minValues[2])
        };
        result.max = {
            static_cast<float>(accessor.maxValues[0]),
            static_cast<float>(accessor.maxValues[1]),
            static_cast<float>(accessor.maxValues[2])
        };

        return result;
    }

    // ModelクラスからAABBを抽出
    AABB AABBExtractor::ExtractFromModel(const Model* model) {
        if (!model) {
            return AABB();
        }

        // ModelクラスからGLTFデータを取得する必要がある
        // ここではモデルの頂点データから計算する簡易実装
        const auto& vertices = model->GetVertices();
        if (vertices.empty()) {
            return AABB();
        }

        Vector3 minPoint = {
            vertices[0].position.x,
            vertices[0].position.y,
            vertices[0].position.z
        };
        Vector3 maxPoint = minPoint;

        for (const auto& vertex : vertices) {
            Vector3 pos = { vertex.position.x, vertex.position.y, vertex.position.z };

            minPoint.x = std::min(minPoint.x, pos.x);
            minPoint.y = std::min(minPoint.y, pos.y);
            minPoint.z = std::min(minPoint.z, pos.z);

            maxPoint.x = std::max(maxPoint.x, pos.x);
            maxPoint.y = std::max(maxPoint.y, pos.y);
            maxPoint.z = std::max(maxPoint.z, pos.z);
        }

        return AABB(minPoint, maxPoint);
    }

    // AnimatedModelクラスからAABBを抽出
    AABB AABBExtractor::ExtractFromAnimatedModel(const AnimatedModel* model) {
        if (!model) {
            return AABB();
        }

        // AnimatedModelはModelを継承しているので同じ方法で取得
        return ExtractFromModel(static_cast<const Model*>(model));
    }

    // AnimatedModelから複数メッシュのAABBを抽出
    std::vector<AABB> AABBExtractor::ExtractMultipleAABBsFromAnimatedModel(const AnimatedModel* model) {
        std::vector<AABB> aabbs;

        if (!model) {
            return aabbs;
        }

        const auto& modelData = model->GetModelData();

        // マルチマテリアルデータがあれば、各マテリアルごとにAABBを計算
        if (!modelData.matVertexData.empty()) {
            for (const auto& [materialName, matData] : modelData.matVertexData) {
                if (matData.vertices.empty()) continue;

                // 各メッシュの最小・最大を正しく初期化
                Vector3 minPoint = {
                    matData.vertices[0].position.x,
                    matData.vertices[0].position.y,
                    matData.vertices[0].position.z
                };
                Vector3 maxPoint = minPoint;

                // このメッシュの全頂点をチェック
                for (const auto& vertex : matData.vertices) {
                    Vector3 pos = { vertex.position.x, vertex.position.y, vertex.position.z };

                    minPoint.x = std::min(minPoint.x, pos.x);
                    minPoint.y = std::min(minPoint.y, pos.y);
                    minPoint.z = std::min(minPoint.z, pos.z);

                    maxPoint.x = std::max(maxPoint.x, pos.x);
                    maxPoint.y = std::max(maxPoint.y, pos.y);
                    maxPoint.z = std::max(maxPoint.z, pos.z);
                }

                // 空間メッシュ（非常に小さいか存在しないメッシュ）をスキップ
                Vector3 size = {
                    maxPoint.x - minPoint.x,
                    maxPoint.y - minPoint.y,
                    maxPoint.z - minPoint.z
                };

                // サイズが極端に小さい場合はスキップ（閾値: 0.01）
                const float minSize = 0.01f;
                if (size.x < minSize && size.y < minSize && size.z < minSize) {
                    continue;
                }

                aabbs.push_back(AABB(minPoint, maxPoint));
            }
        } else {
            // マルチマテリアルデータがない場合は単一AABBを返す
            aabbs.push_back(ExtractFromModel(static_cast<const Model*>(model)));
        }

        return aabbs;
    }

    // CollisionObject3Dの実装
    CollisionObject3D::CollisionObject3D(Object3d* object, const AABB& localAABB, const std::string& name)
        : object_(object), localAABB_(localAABB), worldAABB_(), enabled_(true), name_(name) {
        Update();
    }

    void CollisionObject3D::Update() {
        if (!object_) return;

        // Object3dから位置とスケールを取得
        Vector3 position = object_->GetPosition();
        Vector3 scale = object_->GetScale();

        // 回転は今回は考慮しない(AABBは軸並行のため)
        // より正確な衝突判定が必要な場合はOBB(Oriented Bounding Box)を使用する
        worldAABB_ = TransformAABB(localAABB_, position, scale);
    }

    // AABBCollisionManagerの実装
    AABBCollisionManager* AABBCollisionManager::GetInstance() {
        return instance_;
    }

    void AABBCollisionManager::Create() {
        if (!instance_) {
            instance_ = new AABBCollisionManager();
        }
    }

    void AABBCollisionManager::Destroy() {
        if (instance_) {
            delete instance_;
            instance_ = nullptr;
        }
    }

    void AABBCollisionManager::RegisterObject(Object3d* object, const AABB& localAABB, bool enabled, const std::string& name) {
        if (!object) return;

        // 既に登録されているか確認
        auto existing = FindCollisionObject(object);
        if (existing) {
            existing->SetLocalAABB(localAABB);
            existing->SetEnabled(enabled);
            existing->SetName(name);
            return;
        }

        // 新規登録
        auto collisionObj = std::make_shared<CollisionObject3D>(object, localAABB, name);
        collisionObj->SetEnabled(enabled);
        collisionObjects_.push_back(collisionObj);
    }

    void AABBCollisionManager::RegisterObjectWithMultipleAABBs(Object3d* object, const std::vector<AABB>& localAABBs, bool enabled, const std::string& name) {
        if (!object || localAABBs.empty()) return;

        // 各AABBごとに個別のCollisionObjectを作成
        for (size_t i = 0; i < localAABBs.size(); ++i) {
            std::string meshName = name + "_Mesh" + std::to_string(i);
            auto collisionObj = std::make_shared<CollisionObject3D>(object, localAABBs[i], meshName);
            collisionObj->SetEnabled(enabled);
            collisionObjects_.push_back(collisionObj);
        }
    }

    void AABBCollisionManager::UnregisterObject(Object3d* object) {
        collisionObjects_.erase(
            std::remove_if(collisionObjects_.begin(), collisionObjects_.end(),
                [object](const std::shared_ptr<CollisionObject3D>& obj) {
                    return obj->GetObject() == object;
                }),
            collisionObjects_.end()
        );
    }

    void AABBCollisionManager::Clear() {
        collisionObjects_.clear();
    }

    void AABBCollisionManager::Update() {
        // 全オブジェクトのワールドAABBを更新
        for (auto& obj : collisionObjects_) {
            if (obj->IsEnabled()) {
                obj->Update();
            }
        }

        // 衝突リストをクリア
        currentCollisions_.clear();

        // 衝突判定
        for (size_t i = 0; i < collisionObjects_.size(); ++i) {
            if (!collisionObjects_[i]->IsEnabled()) continue;

            for (size_t j = i + 1; j < collisionObjects_.size(); ++j) {
                if (!collisionObjects_[j]->IsEnabled()) continue;

                if (CheckAABBCollision(
                    collisionObjects_[i]->GetWorldAABB(),
                    collisionObjects_[j]->GetWorldAABB())) {

                    // 衝突ペアを記録
                    currentCollisions_.push_back({
                        collisionObjects_[i],
                        collisionObjects_[j]
                    });

                    // コールバック実行
                    if (collisionCallback_) {
                        collisionCallback_(
                            collisionObjects_[i]->GetObject(),
                            collisionObjects_[j]->GetObject()
                        );
                    }
                }
            }
        }
    }

    std::shared_ptr<CollisionObject3D> AABBCollisionManager::FindCollisionObject(Object3d* object) {
        auto it = std::find_if(collisionObjects_.begin(), collisionObjects_.end(),
            [object](const std::shared_ptr<CollisionObject3D>& obj) {
                return obj->GetObject() == object;
            });

        return (it != collisionObjects_.end()) ? *it : nullptr;
    }

    void AABBCollisionManager::DrawImGui() {
#ifdef _DEBUG
        ImGui::Begin("AABB Collision Debug");

        ImGui::Text("Registered Objects: %zu", collisionObjects_.size());
        ImGui::Text("Active Collisions: %zu", currentCollisions_.size());
        ImGui::Separator();

        // 登録されているオブジェクト一覧
        if (ImGui::CollapsingHeader("Registered Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (size_t i = 0; i < collisionObjects_.size(); ++i) {
                const auto& colObj = collisionObjects_[i];
                const auto& worldAABB = colObj->GetWorldAABB();

                ImGui::PushID(static_cast<int>(i));

                bool enabled = colObj->IsEnabled();
                ImGui::Checkbox("Enabled", &enabled);
                ImGui::SameLine();

                // 名前がある場合は名前を表示、ない場合はインデックス
                if (!colObj->GetName().empty()) {
                    ImGui::Text("%s", colObj->GetName().c_str());
                } else {
                    ImGui::Text("Object %zu", i);
                }

                ImGui::Indent();
                ImGui::Text("Min: (%.2f, %.2f, %.2f)", worldAABB.min.x, worldAABB.min.y, worldAABB.min.z);
                ImGui::Text("Max: (%.2f, %.2f, %.2f)", worldAABB.max.x, worldAABB.max.y, worldAABB.max.z);
                ImGui::Unindent();

                ImGui::PopID();
            }
        }

        ImGui::Separator();

        // 衝突ペア一覧
        if (ImGui::CollapsingHeader("Active Collisions", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (currentCollisions_.empty()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No collisions detected");
            } else {
                for (size_t i = 0; i < currentCollisions_.size(); ++i) {
                    const auto& pair = currentCollisions_[i];

                    // CollisionObject3Dから直接名前を取得
                    std::string nameA = pair.objA->GetName().empty() ? "Unknown" : pair.objA->GetName();
                    std::string nameB = pair.objB->GetName().empty() ? "Unknown" : pair.objB->GetName();

                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                        "%s <-> %s", nameA.c_str(), nameB.c_str());
                }
            }
        }

        ImGui::End();
#endif
    }

} // namespace Collision
