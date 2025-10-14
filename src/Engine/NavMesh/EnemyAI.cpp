#include "EnemyAI.h"
#include <cmath>
#include <algorithm>
#include <numbers>

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
    , debugMode_(false) {
}

EnemyAI::~EnemyAI() {
}

void EnemyAI::Initialize(NavMeshSystem* navMeshSystem) {
    navMeshSystem_ = navMeshSystem;
    currentState_ = EnemyState::Idle;
    currentPath_.clear();
    currentWaypointIndex_ = 0;
}

void EnemyAI::Update(float deltaTime, const Vector3& playerPosition) {
    if (!navMeshSystem_ || !navMeshSystem_->IsValid()) {
        return;
    }

    // タイマー更新
    pathUpdateTimer_ += deltaTime;
    losCheckTimer_ += deltaTime;

    // ステート更新
    UpdateState(playerPosition);

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
                // 巡回ロジック（未実装）
                break;

            case EnemyState::Search:
                // 最後に見た位置へ移動
                break;

            default:
                break;
        }
    }

    // パスに沿って移動
    if (currentState_ == EnemyState::Chase || currentState_ == EnemyState::Patrol) {
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
            // プレイヤーが範囲外に出たら捜索モードへ
            if (distanceToPlayer > detectionRange_ * 1.5f) {
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
