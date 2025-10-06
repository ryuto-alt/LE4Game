# フラスタムカリング使用方法

## 概要
フラスタムカリング（視錐台カリング）は、カメラの視野外にあるオブジェクトを描画前に除外することで、レンダリングパフォーマンスを向上させる技術です。

## 実装内容

### 新規追加ファイル
- `src/Engine/Camera/Frustum.h` - フラスタムクラスのヘッダー
- `src/Engine/Camera/Frustum.cpp` - フラスタムクラスの実装

### 変更ファイル
- `src/Engine/Camera/Camera.h` - フラスタム機能の追加
- `src/Engine/Camera/Camera.cpp` - フラスタム計算の実装

## 使用方法

### 1. 基本的な使い方

カメラクラスは毎フレーム自動的にフラスタムを更新します。以下のメソッドを使ってカリングテストを実行できます。

```cpp
// カメラの取得（例）
Camera* camera = Object3dCommon::GetDefaultCamera();

// 点がカメラの視野内にあるかチェック
Vector3 point = {0.0f, 0.0f, 0.0f};
bool isVisible = camera->IsPointInFrustum(point);

// AABBバウンディングボックスが視野内にあるかチェック
Vector3 min = {-1.0f, -1.0f, -1.0f};
Vector3 max = {1.0f, 1.0f, 1.0f};
bool isVisible = camera->IsAABBInFrustum(min, max);

// 球が視野内にあるかチェック
Vector3 center = {0.0f, 0.0f, 0.0f};
float radius = 1.0f;
bool isVisible = camera->IsSphereInFrustum(center, radius);
```

### 2. オブジェクトの描画時にカリングを適用

```cpp
// GamePlayScene.cpp などでの使用例
void GamePlayScene::Draw() {
    Camera* camera = Object3dCommon::GetDefaultCamera();

    for (auto& object : objects_) {
        // オブジェクトの位置とスケールから簡易的なバウンディングボックスを計算
        Vector3 pos = object->GetPosition();
        Vector3 scale = object->GetScale();

        // バウンディングボックスの最小・最大点
        Vector3 min = pos - scale;
        Vector3 max = pos + scale;

        // カリングテスト
        if (camera->IsAABBInFrustum(min, max)) {
            // 視野内にある場合のみ描画
            object->Draw();
        }
    }
}
```

### 3. 球を使った簡易的なカリング

```cpp
void GamePlayScene::Draw() {
    Camera* camera = Object3dCommon::GetDefaultCamera();

    for (auto& object : objects_) {
        Vector3 pos = object->GetPosition();
        Vector3 scale = object->GetScale();

        // スケールの最大値を半径とする
        float radius = std::max(std::max(scale.x, scale.y), scale.z);

        // カリングテスト
        if (camera->IsSphereInFrustum(pos, radius)) {
            object->Draw();
        }
    }
}
```

## パフォーマンスの考慮事項

1. **カリングテストのコスト**: カリングテスト自体にもコストがかかるため、少数のオブジェクトしかない場合は効果が薄い可能性があります。

2. **バウンディングボックスの精度**: 精密なバウンディングボックスほど正確ですが、計算コストが高くなります。ゲームの状況に応じて適切な精度を選択してください。

3. **オブジェクトの数**: 数百〜数千のオブジェクトがある場合に特に効果的です。

## テクニカルノート

### フラスタム平面の抽出
フラスタムの6つの平面（左、右、上、下、近、遠）は、ビュープロジェクション行列から数学的に抽出されます。この実装では Gribb & Hartmann のアルゴリズムを使用しています。

### カリングテストの最適化
- **AABB**: p-vertex（positive vertex）法を使用して、6回の平面テストで判定
- **球**: 中心点からの距離テストで簡潔に判定
- **点**: 最も単純な6回の平面テスト

## 今後の拡張

以下の機能を追加することで、さらに高度なカリングシステムを構築できます：

1. **オクルージョンカリング**: 他のオブジェクトに隠れたオブジェクトのカリング
2. **階層的カリング**: シーングラフを使った効率的なカリング
3. **LOD（Level of Detail）**: 距離に応じた詳細度の調整
4. **バッチング**: カリング結果に基づいた描画バッチの最適化
