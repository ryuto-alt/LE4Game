#pragma once
#include "NavMeshSystem.h"
#include "../Math/Mymath.h"
#include <vector>
#include <memory>
#include <string>

// 敵AIのステート
enum class EnemyState {
    Idle,       // 待機
    Patrol,     // 巡回
    Chase,      // 追跡
    Attack,     // 攻撃
    Search      // 捜索
};

// 敵AIクラス: mdファイルのベストプラクティスに基づく実装
class EnemyAI {
public:
    EnemyAI();
    ~EnemyAI();

    // 初期化
    void Initialize(NavMeshSystem* navMeshSystem);

    // 更新（mdファイル推奨: 0.1～0.5秒間隔でパス更新）
    void Update(float deltaTime, const Vector3& playerPosition);

    // 現在の状態取得
    EnemyState GetState() const { return currentState_; }
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetTargetPosition() const { return targetPosition_; }

    // 位置設定
    void SetPosition(const Vector3& position);

    // パラメータ設定（mdファイルより）
    void SetDetectionRange(float range) { detectionRange_ = range; }
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    void SetUpdateRate(float rate) { pathUpdateRate_ = rate; }

    // パスの取得（デバッグ用）
    const std::vector<Vector3>& GetCurrentPath() const { return currentPath_; }

    // 現在のウェイポイントインデックス
    int GetCurrentWaypointIndex() const { return currentWaypointIndex_; }

    // 移動方向取得
    Vector3 GetMoveDirection() const;

    // 回転角度取得（Y軸）
    float GetRotationY() const;

    // プレイヤーが視界内か（LOS: Line of Sight チェック）
    bool IsPlayerVisible(const Vector3& playerPosition) const;

    // 徘徊モード有効化
    void EnablePatrolMode(bool enable) { patrolModeEnabled_ = enable; }
    bool IsPatrolModeEnabled() const { return patrolModeEnabled_; }

    // 徘徊範囲設定
    void SetPatrolRadius(float radius) { patrolRadius_ = radius; }
    void SetPatrolCenter(const Vector3& center) { patrolCenter_ = center; }

private:
    // ステート更新
    void UpdateState(const Vector3& playerPosition);

    // パスファインディング
    void UpdatePathfinding(const Vector3& targetPosition);

    // パスに沿って移動
    void FollowPath(float deltaTime);

    // 視線チェック（mdファイル: LOSチェックの最適化実装）
    bool CheckLineOfSight(const Vector3& target) const;

    // プレイヤーとの距離計算
    float DistanceToPlayer(const Vector3& playerPosition) const;

    // 徘徊用: ランダムなNavMesh上の点を取得
    Vector3 GetRandomPatrolPoint();

    // 徘徊用: 次の徘徊ポイントを選択
    void SelectNextPatrolPoint();

    // ナビメッシュシステム
    NavMeshSystem* navMeshSystem_;

    // 位置と移動
    Vector3 position_;
    Vector3 targetPosition_;
    Vector3 velocity_;
    std::vector<Vector3> currentPath_;
    int currentWaypointIndex_;

    // AI状態
    EnemyState currentState_;
    EnemyState previousState_;

    // パラメータ（mdファイルより推奨値）
    float detectionRange_;      // 検知範囲: 20m推奨
    float moveSpeed_;           // 移動速度
    float pathUpdateRate_;      // パス更新間隔: 0.1～0.5秒推奨
    float pathUpdateTimer_;     // パス更新タイマー

    // 視線チェックパラメータ
    float fieldOfView_;         // 視野角: 110度推奨
    float losCheckInterval_;    // LOSチェック間隔: 0.1～0.3秒推奨
    float losCheckTimer_;       // LOSチェックタイマー

    // ステアリング（mdファイル: ローカルステアリング技術）
    float arrivalRadius_;       // 到着判定半径
    float waypointRadius_;      // ウェイポイント到達判定半径

    // 徘徊パラメータ
    bool patrolModeEnabled_;    // 徘徊モード有効フラグ
    Vector3 patrolCenter_;      // 徘徊の中心位置
    float patrolRadius_;        // 徘徊範囲の半径
    Vector3 currentPatrolTarget_; // 現在の徘徊目標地点
    float patrolWaitTimer_;     // 徘徊ポイント到着後の待機タイマー
    float patrolWaitDuration_;  // 徘徊ポイントでの待機時間（ランダム化）
    std::vector<Vector3> recentPatrolPoints_; // 最近訪れたパトロールポイント履歴
    const int MAX_PATROL_HISTORY = 5; // 履歴保持数

    // スタック検出
    Vector3 lastStuckCheckPosition_; // スタック検出用の前回位置
    float stuckCheckTimer_;     // スタック検出タイマー
    float stuckCheckInterval_;  // スタック検出間隔
    int stuckCounter_;          // スタック回数カウンター

    // デバッグ
    bool debugMode_;
};
