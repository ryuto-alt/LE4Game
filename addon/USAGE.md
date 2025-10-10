# ナビメッシュシステムの使い方

## ワークフロー

### 1. Blenderでナビメッシュを作成

1. **床メッシュの準備**
   - ゲームの床となるメッシュを作成
   - 敵が歩き回るエリアを定義

2. **アドオンのインストール**
   - Edit > Preferences > Add-ons
   - "Install" をクリック
   - `addon` フォルダをZIP圧縮してインストール
   - "Game Engine: NavMesh Generator" を有効化

3. **ナビメッシュ生成**
   - 床メッシュを選択
   - 右サイドバー > NavMesh タブ
   - パラメータを設定：
     ```
     Agent Radius: 0.5    # 敵の半径（メートル）
     Agent Height: 2.0    # 敵の高さ
     Max Slope: 45.0      # 最大傾斜角度
     ```
   - "Generate NavMesh" ボタンをクリック

4. **エクスポート**
   - Export Path を設定（例: `//navmesh.json`）
   - "Export NavMesh" ボタンをクリック
   - `Resources/NavMesh/` フォルダに保存推奨

### 2. C++側での使用

#### Enemy.h に追加

```cpp
#include "NavMesh.h"

class Enemy {
private:
    // 既存のメンバ変数に追加
    NavMesh* navMesh_;
    std::vector<Vector3> currentPath_;
    int currentWaypointIndex_;
};
```

#### GamePlayScene.cpp でナビメッシュを読み込み

```cpp
// ナビメッシュの読み込み
NavMesh* navMesh = new NavMesh();
if (!navMesh->LoadFromFile("Resources/NavMesh/navmesh.json")) {
    OutputDebugStringA("Failed to load navmesh\n");
}

// Enemyに設定
enemy_->SetNavMesh(navMesh);
```

#### Enemy.cpp での使用例

```cpp
void Enemy::SetNavMesh(NavMesh* navMesh) {
    navMesh_ = navMesh;
    currentWaypointIndex_ = 0;
}

void Enemy::Update() {
    if (!player_ || !navMesh_) return;

    Vector3 playerPos = player_->GetPosition();
    float distanceToPlayer = /* 距離計算 */;

    if (distanceToPlayer < detectionRange_) {
        // 現在のパスが無効、またはプレイヤーが移動した場合
        if (currentPath_.empty() || /* プレイヤーが移動判定 */) {
            // 新しいパスを計算
            currentPath_ = navMesh_->FindPath(position_, playerPos);
            currentWaypointIndex_ = 0;
        }

        // パスに沿って移動
        if (currentWaypointIndex_ < currentPath_.size()) {
            Vector3 targetWaypoint = currentPath_[currentWaypointIndex_];
            Vector3 toWaypoint = {
                targetWaypoint.x - position_.x,
                0.0f,
                targetWaypoint.z - position_.z
            };

            float distToWaypoint = std::sqrt(
                toWaypoint.x * toWaypoint.x +
                toWaypoint.z * toWaypoint.z
            );

            // ウェイポイントに到達したら次へ
            if (distToWaypoint < 1.0f) {
                currentWaypointIndex_++;
            } else {
                // ウェイポイントに向かって移動
                toWaypoint.x /= distToWaypoint;
                toWaypoint.z /= distToWaypoint;

                position_.x += toWaypoint.x * moveSpeed_;
                position_.z += toWaypoint.z * moveSpeed_;

                currentRotationY_ = std::atan2(toWaypoint.x, toWaypoint.z);
            }
        }
    }
}
```

## パスファインディングのアルゴリズム

実装されているA*アルゴリズムの動作：

1. **スタート位置の三角形を検索**
2. **ゴール位置の三角形を検索**
3. **A*で最短経路を計算**
   - f(n) = g(n) + h(n)
   - g(n): スタートからの実コスト
   - h(n): ゴールまでのヒューリスティック（直線距離）
4. **三角形の中心点を経由するパスを生成**

## Tips

### パフォーマンス最適化

- パス計算は重い処理なので、毎フレーム実行しない
- プレイヤーが一定距離以上移動したときのみ再計算
- タイマーで一定間隔（例：0.5秒）ごとに更新

```cpp
// Enemy.h
float pathUpdateTimer_;
Vector3 lastPlayerPosition_;

// Enemy.cpp
void Enemy::Update() {
    pathUpdateTimer_ -= deltaTime;

    if (pathUpdateTimer_ <= 0.0f) {
        Vector3 playerPos = player_->GetPosition();
        float playerMoved = /* 前回位置との距離 */;

        if (playerMoved > 5.0f) { // 5メートル以上移動
            currentPath_ = navMesh_->FindPath(position_, playerPos);
            currentWaypointIndex_ = 0;
            lastPlayerPosition_ = playerPos;
        }

        pathUpdateTimer_ = 0.5f; // 0.5秒ごとにチェック
    }
}
```

### デバッグ表示

ナビメッシュとパスをデバッグ表示する場合：

```cpp
// ナビメッシュの三角形を描画
for (const auto& tri : navMesh_->GetTriangles()) {
    const auto& vertices = navMesh_->GetVertices();
    // 三角形のエッジを線で描画
}

// 現在のパスを描画
for (size_t i = 0; i < currentPath_.size() - 1; i++) {
    // ウェイポイント間を線で描画
}
```

## トラブルシューティング

### パスが見つからない

- スタート/ゴール位置がナビメッシュ上にあるか確認
- ナビメッシュが連続しているか確認（島に分かれていないか）
- Max Slope の値を調整

### 敵が壁にぶつかる

- Agent Radius を大きくする
- ナビメッシュを再生成

### パフォーマンスが悪い

- ナビメッシュの三角形数を減らす（Blenderでデシメート）
- パス更新頻度を下げる
