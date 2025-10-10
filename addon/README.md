# NavMesh Generator for Blender

**床と障害物から自動でナビメッシュ生成！視覚的にわかりやすい色分け**

- **青色 = 通れる（ウォーカブル）**
- **赤色 = 通れない（障害物）**

## インストール方法

1. Blenderを起動
2. Edit > Preferences > Add-ons
3. "Install" ボタンをクリック
4. `addon` フォルダをZIP圧縮してインストール
5. "Game Engine: NavMesh Generator" を有効化

## 使い方（超簡単4ステップ）

### Step 1: 床を選択

1. 床となるメッシュオブジェクトを選択
2. NavMeshパネル（右サイドバー）で **"Set as Floor"** をクリック

### Step 2: 障害物を配置

1. 壁や障害物となるメッシュオブジェクトを配置
2. **自動検出されます**（床以外のすべてのメッシュ）

### Step 3: ナビメッシュ生成

1. **"Generate NavMesh"** をクリック
2. 自動的に2つのメッシュが生成される：
   - **NavMesh_Walkable（青色）**: 通れる場所
   - **NavMesh_Blocked（赤色）**: 通れない場所

### Step 4: エクスポート

1. Export Pathを設定（例: `//navmesh.json`）
2. **"Export NavMesh"** をクリック
3. ウォーカブルなナビメッシュのみがエクスポートされる

## パネルの機能

### Step 1: Select Floor
- **Floor Object**: 床オブジェクト（ドロップダウンまたはSet as Floor）
- **Set as Floor**: 選択したオブジェクトを床として設定

### Step 2: Settings
- **Agent Radius**: エージェント（敵）の半径（デフォルト0.5）
- **Agent Height**: エージェントの高さ（デフォルト2.0）
- **Cell Size**: サンプリンググリッドのサイズ（デフォルト0.5）
  - 小さいほど精密だが処理時間が長い

### Step 3: Generate
- **Generate NavMesh**: ナビメッシュを生成
- **Clear NavMesh**: 生成したナビメッシュを削除

### Step 4: Export
- **Export Path**: 保存先パス
- **Export NavMesh**: JSONファイルとしてエクスポート

## 仕組み

### 自動障害物検出

1. **床からレイキャスト（上方向）**
   - 床の各グリッドポイントから上に向けてレイキャスト
   - Agent Height の高さまでチェック

2. **障害物判定**
   - 何も当たらない → **青色（通れる）**
   - 障害物に当たる → **赤色（通れない）**

3. **視覚化**
   - `NavMesh_Walkable`: 青色半透明（通れる場所）
   - `NavMesh_Blocked`: 赤色半透明（通れない場所）

### エクスポートされるデータ

**青色（ウォーカブル）のメッシュのみ**がエクスポートされます。

```json
{
  "version": "1.0",
  "agent_radius": 0.5,
  "agent_height": 2.0,
  "vertices": [
    {"x": 0.0, "y": 0.0, "z": 0.0},
    ...
  ],
  "triangles": [
    {
      "indices": [0, 1, 2],
      "center": {"x": 0.0, "y": 0.0, "z": 0.0}
    },
    ...
  ],
  "adjacency": {
    "0": [1, 2],
    ...
  }
}
```

## ワークフロー例

### 迷路を作る

```
1. 大きな平面を作成（Shift + A → Mesh → Plane → S → 20でスケール）
2. Set as Floor ボタン
3. 壁を配置（Shift + A → Mesh → Cube → 位置調整）
4. 壁を複製して迷路を作る（Shift + D）
5. Generate NavMesh
   → 青色（通路）、赤色（壁の上）が表示される
6. Export NavMesh
```

### 複雑な地形

```
1. 床メッシュを選択 → Set as Floor
2. Settings:
   - Agent Radius: 0.5
   - Agent Height: 2.0（キャラクターの高さ）
   - Cell Size: 0.3（細かくしたい場合）
3. 障害物（岩、木、建物など）を配置
4. Generate NavMesh
   → 障害物を避けた青色のナビメッシュが生成される
5. Export
```

## 設定のヒント

### Cell Size の調整（重要！）

- **0.3-0.5**: 細かい精度、処理時間長い（小さい迷路向け）
- **1.0**: バランス（おすすめ・デフォルト）
- **2.0-3.0**: 粗い精度、処理時間短い（大きいマップ向け）
- **5.0**: 超高速、非常に粗い（テスト用）

⚡ **パフォーマンスTips:**
- 広いマップなら Cell Size を 2.0～3.0 に設定
- 0.5 → 1.0 にするだけで **約4倍高速化**！
- 0.5 → 2.0 にすると **約16倍高速化**！

### Simplify（エクスポート時の簡略化）

- **0.0**: 簡略化なし（デフォルト）
- **0.3**: 30%削減（やや高速）
- **0.5**: 50%削減（バランス・おすすめ）
- **0.7**: 70%削減（高速、ただし精度低下）
- **1.0**: 最大削減（超高速、精度は大幅低下）

⚡ **エクスポートが重い場合:**
- Simplify を 0.5 に設定すると大幅に高速化
- ゲーム側のA*も速くなる（三角形が減るため）

### Agent Height の調整

- **低すぎる**: 天井の低い場所も通れると判定される
- **ちょうどいい**: キャラクターの実際の高さに合わせる
- **高すぎる**: 通路が狭く検出される

## 座標系変換

- Blender X → Game X
- Blender Y → Game Z
- Blender Z → Game Y

## トラブルシューティング

### "ウォーカブルな領域が見つかりませんでした"

- 床オブジェクトが正しく設定されているか確認
- Cell Size を大きくしてみる（0.5 → 1.0）
- 床が実際に存在するか確認

### 障害物が検出されない

- 障害物オブジェクトが可視状態か確認（目のアイコン）
- Agent Height を調整（高くする）
- 障害物が床より上にあるか確認

### 青色と赤色が逆

- 仕様です
  - **青 = 通れる**
  - **赤 = 通れない（障害物）**

### ナビメッシュが粗い

- Cell Size を小さくする（1.0 → 0.5）
- ただし処理時間が長くなる

### Generate/Exportが重すぎる

**Generate（生成）が重い場合:**
1. Cell Size を大きくする（0.5 → 1.0 or 2.0）
2. 進捗メッセージを確認（10%ずつ表示される）
3. 大きいマップなら Cell Size 2.0～3.0 推奨

**Export（エクスポート）が重い場合:**
1. Simplify を 0.5 に設定（50%削減）
2. Adjacency 計算が自動で最適化される
3. エクスポート後に頂点数・三角形数が表示される

**目安:**
- 10万点以上 → Cell Size を 2.0 以上に
- エクスポートに30秒以上 → Simplify 0.5～0.7

## C++での使用方法

エクスポートしたJSONファイルは、`NavMesh.h`/`NavMesh.cpp`で読み込めます。

```cpp
// GamePlayScene.cpp
NavMesh* navMesh = new NavMesh();
navMesh->LoadFromFile("Resources/NavMesh/navmesh.json");

enemy_->SetNavMesh(navMesh);

// Enemy.cpp
std::vector<Vector3> path = navMesh_->FindPath(position_, playerPos);
```

詳細は `USAGE.md` を参照してください。

## メリット

✅ **ペイント不要** - 障害物を置くだけで自動生成
✅ **視覚的** - 青（通れる）、赤（通れない）で一目瞭然
✅ **シンプル** - 4ステップで完了
✅ **柔軟** - Cell Size で精度調整可能
✅ **正確** - レイキャストベースの障害物検出
