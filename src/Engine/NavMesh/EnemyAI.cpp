#include "EnemyAI.h"
#include <cmath>
#include <algorithm>
#include <numbers>
#include <random>

EnemyAI::EnemyAI()
    : navMeshSystem_(nullptr)
    , position_({0.0f, 0.0f, 0.0f})
    , targetPosition_({0.0f, 0.0f, 0.0f})
    , velocity_({0.0f, 0.0f, 0.0f})
    , currentWaypointIndex_(0)
    , currentState_(EnemyState::Idle)
    , previousState_(EnemyState::Idle)
    , detectionRange_(20.0f)        // mdファイル推奨値
    , moveSpeed_(3.0f)
    , pathUpdateRate_(0.2f)          // mdファイル推奨: 0.1～0.5秒
    , pathUpdateTimer_(0.0f)
    , fieldOfView_(110.0f)           // mdファイル推奨: 110度
    , losCheckInterval_(0.2f)        // mdファイル推奨: 0.1～0.3秒
    , losCheckTimer_(0.0f)
    , arrivalRadius_(1.0f)
    , waypointRadius_(0.5f)
    , patrolModeEnabled_(false)
    , patrolCenter_({0.0f, 0.0f, 0.0f})
    , patrolRadius_(50.0f)           // デフォルト50m範囲
    , currentPatrolTarget_({0.0f, 0.0f, 0.0f})
    , patrolWaitTimer_(0.0f)
    , patrolWaitDuration_(0.0f)
    , lastStuckCheckPosition_({0.0f, 0.0f, 0.0f})
    , stuckCheckTimer_(0.0f)
    , stuckCheckInterval_(2.0f)      // 2秒ごとにスタックチェック
    , stuckCounter_(0)
    , debugMode_(false) {
}

EnemyAI::~EnemyAI() {
}

void EnemyAI::Initialize(NavMeshSystem* navMeshSystem) {
    navMeshSystem_ = navMeshSystem;
    currentState_ = EnemyState::Patrol;  // デフォルトでPatrol状態に
    currentPath_.clear();
    currentWaypointIndex_ = 0;

    // 徘徊の初期化
    patrolCenter_ = position_;  // 現在位置を中心に設定
    patrolModeEnabled_ = true;  // デフォルトで徘徊モード有効
    recentPatrolPoints_.clear();
    lastStuckCheckPosition_ = position_;
    stuckCheckTimer_ = 0.0f;
    stuckCounter_ = 0;

    // 最初の徘徊ポイントを選択
    SelectNextPatrolPoint();
}

void EnemyAI::Update(float deltaTime, const Vector3& playerPosition) {
    if (!navMeshSystem_ || !navMeshSystem_->IsValid()) {
        return;
    }

    // タイマー更新
    pathUpdateTimer_ += deltaTime;
    losCheckTimer_ += deltaTime;
    stuckCheckTimer_ += deltaTime;

    // ステート更新
    UpdateState(playerPosition);

    // スタック検出（2秒ごと）
    if (stuckCheckTimer_ >= stuckCheckInterval_) {
        stuckCheckTimer_ = 0.0f;

        // 前回の位置からの移動距離を計算
        float dx = position_.x - lastStuckCheckPosition_.x;
        float dz = position_.z - lastStuckCheckPosition_.z;
        float movedDistance = std::sqrt(dx * dx + dz * dz);

        // 2秒間で1m以下しか動いていない場合はスタック
        if (movedDistance < 1.0f && (currentState_ == EnemyState::Patrol || currentState_ == EnemyState::Chase)) {
            stuckCounter_++;

            // スタックしている場合は新しいパトロールポイントを選択
            if (currentState_ == EnemyState::Patrol) {
                SelectNextPatrolPoint();
                currentPath_.clear();
                currentWaypointIndex_ = 0;
            }
        } else {
            stuckCounter_ = 0;  // 正常に移動している
        }

        lastStuckCheckPosition_ = position_;
    }

    // パスファインディング更新（mdファイル推奨: 毎フレームではなく間隔を空ける）
    if (pathUpdateTimer_ >= pathUpdateRate_) {
        pathUpdateTimer_ = 0.0f;

        switch (currentState_) {
            case EnemyState::Chase:
                // プレイヤーの予測位置を計算（mdファイル: 予測追跡）
                // 簡易実装: 現在位置をターゲットとする
                UpdatePathfinding(playerPosition);
                break;

            case EnemyState::Patrol:
                // 徘徊モードが有効な場合
                if (patrolModeEnabled_) {
                    // 目標地点への距離をチェック
                    float dx = currentPatrolTarget_.x - position_.x;
                    float dz = currentPatrolTarget_.z - position_.z;
                    float distanceToTarget = std::sqrt(dx * dx + dz * dz);

                    // 待機中の場合
                    if (patrolWaitTimer_ > 0.0f) {
                        patrolWaitTimer_ -= deltaTime;
                        velocity_ = {0.0f, 0.0f, 0.0f};  // 停止

                        if (patrolWaitTimer_ <= 0.0f) {
                            // 待機終了、次のポイントを選択
                            SelectNextPatrolPoint();
                        }
                    }
                    // 目標に到達した場合
                    else if (distanceToTarget < arrivalRadius_) {
                        // ランダムな待機時間を設定（2～5秒）
                        static std::random_device rd;
                        static std::mt19937 gen(rd());
                        std::uniform_real_distribution<float> waitDist(2.0f, 5.0f);
                        patrolWaitTimer_ = waitDist(gen);
                        patrolWaitDuration_ = patrolWaitTimer_;
                    }
                    // 目標に向かって移動中
                    else {
                        UpdatePathfinding(currentPatrolTarget_);
                    }
                }
                break;

            case EnemyState::Search:
                // 最後に見た位置へ移動
                UpdatePathfinding(targetPosition_);
                break;

            default:
                break;
        }
    }

    // パスに沿って移動（待機中でない場合）
    if (currentState_ == EnemyState::Chase ||
        (currentState_ == EnemyState::Patrol && patrolWaitTimer_ <= 0.0f) ||
        currentState_ == EnemyState::Search) {
        FollowPath(deltaTime);
    }

    // 位置をナビメッシュ上に補正
    Vector3 correctedPosition;
    if (navMeshSystem_->GetNearestPoint(position_, correctedPosition)) {
        position_ = correctedPosition;
    }
}

void EnemyAI::UpdateState(const Vector3& playerPosition) {
    float distanceToPlayer = DistanceToPlayer(playerPosition);

    // 状態遷移ロジック
    switch (currentState_) {
        case EnemyState::Idle:
            // プレイヤーが検知範囲内に入ったら追跡開始
            if (distanceToPlayer < detectionRange_ && IsPlayerVisible(playerPosition)) {
                currentState_ = EnemyState::Chase;
            }
            break;

        case EnemyState::Patrol:
            // プレイヤーを発見したら追跡
            if (distanceToPlayer < detectionRange_ && IsPlayerVisible(playerPosition)) {
                currentState_ = EnemyState::Chase;
            }
            break;

        case EnemyState::Chase:
            // プレイヤーが15m以上離れたら捜索モードへ
            if (distanceToPlayer > 15.0f) {
                currentState_ = EnemyState::Search;
                targetPosition_ = playerPosition; // 最後に見た位置を記憶
            }
            // 攻撃範囲内に入ったら攻撃
            else if (distanceToPlayer < 2.0f) {
                currentState_ = EnemyState::Attack;
            }
            break;

        case EnemyState::Attack:
            // プレイヤーが離れたら再び追跡
            if (distanceToPlayer > 3.0f) {
                currentState_ = EnemyState::Chase;
            }
            break;

        case EnemyState::Search:
            // 目標地点に到達したらアイドルに戻る
            float distanceToTarget = std::sqrt(
                (targetPosition_.x - position_.x) * (targetPosition_.x - position_.x) +
                (targetPosition_.z - position_.z) * (targetPosition_.z - position_.z)
            );
            if (distanceToTarget < arrivalRadius_) {
                currentState_ = EnemyState::Idle;
            }
            // プレイヤーを再発見したら追跡再開
            if (distanceToPlayer < detectionRange_ && IsPlayerVisible(playerPosition)) {
                currentState_ = EnemyState::Chase;
            }
            break;
    }

    // ステート変更時のログ
    if (currentState_ != previousState_) {
        previousState_ = currentState_;
    }
}

void EnemyAI::UpdatePathfinding(const Vector3& targetPosition) {
    if (!navMeshSystem_ || !navMeshSystem_->IsValid()) {
        return;
    }

    // パスを計算
    std::vector<Vector3> newPath;
    if (navMeshSystem_->FindPath(position_, targetPosition, newPath)) {
        currentPath_ = newPath;
        currentWaypointIndex_ = 0;
    }
}

void EnemyAI::FollowPath(float deltaTime) {
    if (currentPath_.empty() || currentWaypointIndex_ >= static_cast<int>(currentPath_.size())) {
        velocity_ = {0.0f, 0.0f, 0.0f};
        return;
    }

    // 現在のウェイポイント
    Vector3 currentWaypoint = currentPath_[currentWaypointIndex_];

    // ウェイポイントへの方向
    Vector3 toWaypoint = {
        currentWaypoint.x - position_.x,
        0.0f, // Y軸は無視（地面に沿って移動）
        currentWaypoint.z - position_.z
    };

    float distanceToWaypoint = std::sqrt(toWaypoint.x * toWaypoint.x + toWaypoint.z * toWaypoint.z);

    // ウェイポイントに到達したか
    if (distanceToWaypoint < waypointRadius_) {
        currentWaypointIndex_++;
        if (currentWaypointIndex_ >= static_cast<int>(currentPath_.size())) {
            velocity_ = {0.0f, 0.0f, 0.0f};
            return;
        }
        // 次のウェイポイントへ
        currentWaypoint = currentPath_[currentWaypointIndex_];
        toWaypoint = {
            currentWaypoint.x - position_.x,
            0.0f,
            currentWaypoint.z - position_.z
        };
        distanceToWaypoint = std::sqrt(toWaypoint.x * toWaypoint.x + toWaypoint.z * toWaypoint.z);
    }

    // 方向を正規化
    if (distanceToWaypoint > 0.001f) {
        toWaypoint.x /= distanceToWaypoint;
        toWaypoint.z /= distanceToWaypoint;
    }

    // Arrival behavior (mdファイル: ステアリングビヘイビア)
    float speed = moveSpeed_;

    if (currentWaypointIndex_ == static_cast<int>(currentPath_.size()) - 1) {
        // 最終ウェイポイントに近づいたら減速
        float slowingDistance = 3.0f;
        if (distanceToWaypoint < slowingDistance) {
            speed *= (distanceToWaypoint / slowingDistance);
        }
    }

    // 速度を設定
    velocity_ = {
        toWaypoint.x * speed,
        0.0f,
        toWaypoint.z * speed
    };

    // 位置を更新
    position_.x += velocity_.x * deltaTime;
    position_.z += velocity_.z * deltaTime;
}

bool EnemyAI::CheckLineOfSight(const Vector3& target) const {
    if (!navMeshSystem_ || !navMeshSystem_->IsValid()) {
        return false;
    }

    // ナビメッシュ上でレイキャストを実行
    Vector3 hitPos;
    bool blocked = navMeshSystem_->Raycast(position_, target, hitPos);

    // ヒットしなければ視線が通っている
    return !blocked;
}

bool EnemyAI::IsPlayerVisible(const Vector3& playerPosition) const {
    // LOSチェック（mdファイル: 3段階のチェック）

    // 1. 距離チェック
    float distance = DistanceToPlayer(playerPosition);
    if (distance > detectionRange_) {
        return false;
    }

    // 2. 視野角チェック
    Vector3 toPlayer = {
        playerPosition.x - position_.x,
        0.0f,
        playerPosition.z - position_.z
    };

    // 前方ベクトル（回転から計算）
    float rotY = GetRotationY();
    Vector3 forward = {
        std::sin(rotY),
        0.0f,
        std::cos(rotY)
    };

    // 内積で角度を計算
    float dot = toPlayer.x * forward.x + toPlayer.z * forward.z;
    float lenPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
    float lenForward = std::sqrt(forward.x * forward.x + forward.z * forward.z);

    if (lenPlayer > 0.001f && lenForward > 0.001f) {
        float cosAngle = dot / (lenPlayer * lenForward);
        float angle = std::acos(std::clamp(cosAngle, -1.0f, 1.0f));
        float angleDeg = angle * 180.0f / std::numbers::pi_v<float>;

        if (angleDeg > fieldOfView_ * 0.5f) {
            return false;
        }
    }

    // 3. 障害物チェック（レイキャスト）
    return CheckLineOfSight(playerPosition);
}

float EnemyAI::DistanceToPlayer(const Vector3& playerPosition) const {
    float dx = playerPosition.x - position_.x;
    float dz = playerPosition.z - position_.z;
    return std::sqrt(dx * dx + dz * dz);
}

void EnemyAI::SetPosition(const Vector3& position) {
    position_ = position;
}

Vector3 EnemyAI::GetMoveDirection() const {
    if (velocity_.x == 0.0f && velocity_.z == 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }

    float length = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
    if (length > 0.001f) {
        return {velocity_.x / length, 0.0f, velocity_.z / length};
    }

    return {0.0f, 0.0f, 0.0f};
}

float EnemyAI::GetRotationY() const {
    Vector3 dir = GetMoveDirection();
    if (dir.x == 0.0f && dir.z == 0.0f) {
        return 0.0f;
    }

    return std::atan2(dir.x, dir.z);
}

// ランダムなNavMesh上の点を取得（重複回避付き）
Vector3 EnemyAI::GetRandomPatrolPoint() {
    if (!navMeshSystem_ || !navMeshSystem_->IsValid()) {
        return position_;
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());

    const int MAX_ATTEMPTS = 20;  // 最大試行回数
    const float MIN_DISTANCE_FROM_RECENT = 15.0f;  // 最近訪れた場所から15m以上離れる

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        // 半径内のランダムな角度と距離
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * std::numbers::pi_v<float>);
        std::uniform_real_distribution<float> radiusDist(patrolRadius_ * 0.3f, patrolRadius_);  // 30%～100%の範囲

        float angle = angleDist(gen);
        float distance = radiusDist(gen);

        // 候補位置を計算
        Vector3 candidatePoint = {
            patrolCenter_.x + distance * std::cos(angle),
            patrolCenter_.y,
            patrolCenter_.z + distance * std::sin(angle)
        };

        // NavMesh上の最も近い有効な点を取得
        Vector3 validPoint;
        if (!navMeshSystem_->GetNearestPoint(candidatePoint, validPoint)) {
            continue;  // 有効な点が見つからない場合は次の試行へ
        }

        // 最近訪れた場所との距離をチェック
        bool tooCloseToRecent = false;
        for (const Vector3& recentPoint : recentPatrolPoints_) {
            float dx = validPoint.x - recentPoint.x;
            float dz = validPoint.z - recentPoint.z;
            float distToRecent = std::sqrt(dx * dx + dz * dz);

            if (distToRecent < MIN_DISTANCE_FROM_RECENT) {
                tooCloseToRecent = true;
                break;
            }
        }

        // 重複していない場合、この点を使用
        if (!tooCloseToRecent) {
            return validPoint;
        }
    }

    // 全ての試行が失敗した場合、履歴をクリアして再試行
    recentPatrolPoints_.clear();
    return GetRandomPatrolPoint();
}

// 次の徘徊ポイントを選択
void EnemyAI::SelectNextPatrolPoint() {
    // 新しいランダムポイントを取得
    currentPatrolTarget_ = GetRandomPatrolPoint();

    // 履歴に追加
    recentPatrolPoints_.push_back(currentPatrolTarget_);

    // 履歴が上限を超えたら古いものから削除
    if (static_cast<int>(recentPatrolPoints_.size()) > MAX_PATROL_HISTORY) {
        recentPatrolPoints_.erase(recentPatrolPoints_.begin());
    }

    // パスをクリアして再計算を促す
    currentPath_.clear();
    currentWaypointIndex_ = 0;
}
