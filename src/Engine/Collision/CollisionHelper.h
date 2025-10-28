#pragma once
#include "AABBCollision.h"
#include "Object3d.h"

namespace Collision {

/// <summary>
/// 衝突応答処理を行うヘルパークラス
/// </summary>
class CollisionHelper {
public:
	/// <summary>
	/// AABB衝突応答による押し出し処理
	/// </summary>
	/// <param name="position">オブジェクトの位置（参照渡しで更新される）</param>
	/// <param name="object3d">対象のObject3d</param>
	/// <param name="ignoreNames">衝突を無視するオブジェクト名のリスト</param>
	/// <param name="maxIterations">最大反復回数</param>
	/// <param name="pushoutMargin">押し出しマージン</param>
	static void HandleAABBPushout(
		Vector3& position,
		Object3d* object3d,
		const std::vector<std::string>& ignoreNames = {},
		int maxIterations = 5,
		float pushoutMargin = 0.1f
	);
};

} // namespace Collision
