#pragma once
#include "scene/SceneFactory.h"

// ゲームシーンの生成を行うファクトリークラス
class GameSceneFactory : public SceneFactory {
public:
    // コンストラクタ・デストラクタ
    GameSceneFactory() = default;
    ~GameSceneFactory() = default;

    // シーン生成メソッドのオーバーライド
    std::unique_ptr<IScene> CreateScene(const std::string& sceneName) override;
};
