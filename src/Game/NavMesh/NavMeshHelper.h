#pragma once
#include "Mymath.h"
#include <vector>

// Forward declarations
class NavMesh;

/// <summary>
/// NavMeshパス追従処理を行うヘルパークラス
/// </summary>
class NavMeshHelper {
public:
	/// <summary>
	/// NavMesh上の有効な位置に補正
	/// </summary>
	/// <param name="position">補正する位置</param>
	/// <param name="navMesh">NavMeshインスタンス</param>
	/// <param name="extents">探索範囲</param>
	/// <returns>補正された位置</returns>
	static Vector3 ClampToNavMesh(
		const Vector3& position,
		NavMesh* navMesh,
		const Vector3& extents = {2.0f, 4.0f, 2.0f}
	);

	/// <summary>
	/// パスに沿って移動する処理（先読みで滑らかに）
	/// </summary>
	/// <param name="position">現在位置（更新される）</param>
	/// <param name="currentRotationY">現在の回転Y（更新される）</param>
	/// <param name="currentSpeed">現在の速度（更新される）</param>
	/// <param name="currentPath">ウェイポイントのリスト</param>
	/// <param name="currentWaypointIndex">現在のウェイポイントインデックス（更新される）</param>
	/// <param name="moveSpeed">移動速度</param>
	/// <param name="deltaTime">デルタタイム（フレーム間の経過時間）</param>
	/// <param name="navMesh">NavMeshインスタンス（nullptrでもOK）</param>
	/// <param name="isAtCorner">角を曲がっているか（出力）</param>
	/// <param name="cornerSlowdownFactor">角での減速率（出力）</param>
	static void FollowPath(
		Vector3& position,
		float& currentRotationY,
		float& currentSpeed,
		std::vector<Vector3>& currentPath,
		int& currentWaypointIndex,
		float moveSpeed,
		float deltaTime,
		NavMesh* navMesh = nullptr,
		bool* isAtCorner = nullptr,
		float* cornerSlowdownFactor = nullptr
	);
};
