#include "CollisionHelper.h"
#include <algorithm>

namespace Collision {

void CollisionHelper::HandleAABBPushout(
	Vector3& position,
	Object3d* object3d,
	const std::vector<std::string>& ignoreNames,
	int maxIterations,
	float pushoutMargin
) {
	auto* collisionManager = AABBCollisionManager::GetInstance();
	if (!collisionManager || !object3d) return;

	auto colObj = collisionManager->FindCollisionObject(object3d);
	if (!colObj || !colObj->IsEnabled()) return;

	for (int iteration = 0; iteration < maxIterations; ++iteration) {
		bool hadCollision = false;
		const AABB& myAABB = colObj->GetWorldAABB();

		for (const auto& otherColObj : collisionManager->GetCollisionObjects()) {
			if (otherColObj.get() == colObj.get()) continue;
			if (!otherColObj->IsEnabled()) continue;

			// 無視リストにある名前はスキップ
			bool shouldIgnore = false;
			for (const auto& ignoreName : ignoreNames) {
				if (otherColObj->GetName() == ignoreName) {
					shouldIgnore = true;
					break;
				}
			}
			if (shouldIgnore) continue;

			const AABB& otherAABB = otherColObj->GetWorldAABB();

			if (CheckAABBCollision(myAABB, otherAABB)) {
				hadCollision = true;

				// 押し出しベクトルを計算
				Vector3 overlapMin = {
					(std::max)(myAABB.min.x, otherAABB.min.x),
					(std::max)(myAABB.min.y, otherAABB.min.y),
					(std::max)(myAABB.min.z, otherAABB.min.z)
				};
				Vector3 overlapMax = {
					(std::min)(myAABB.max.x, otherAABB.max.x),
					(std::min)(myAABB.max.y, otherAABB.max.y),
					(std::min)(myAABB.max.z, otherAABB.max.z)
				};

				Vector3 overlap = {
					overlapMax.x - overlapMin.x,
					overlapMax.y - overlapMin.y,
					overlapMax.z - overlapMin.z
				};

				// 最小の押し出し方向を選択（Y軸は無視）
				Vector3 pushOut = {0.0f, 0.0f, 0.0f};
				if (overlap.x < overlap.z) {
					if (myAABB.GetCenter().x < otherAABB.GetCenter().x) {
						pushOut.x = -(overlap.x + pushoutMargin);
					} else {
						pushOut.x = overlap.x + pushoutMargin;
					}
				} else {
					if (myAABB.GetCenter().z < otherAABB.GetCenter().z) {
						pushOut.z = -(overlap.z + pushoutMargin);
					} else {
						pushOut.z = overlap.z + pushoutMargin;
					}
				}

				// 位置を補正
				position.x += pushOut.x;
				position.z += pushOut.z;

				object3d->SetPosition(position);
				object3d->Update();

				colObj->Update();
				break;
			}
		}

		if (!hadCollision) break;
	}
}

} // namespace Collision
