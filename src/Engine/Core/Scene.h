// Scene.h
// シーン基本クラス
#pragma once

#include "../../Game/scene/IScene.h"

// シーン基本クラス
class Scene : public IScene {
public:
    // 仮想デストラクタ
    virtual ~Scene() = default;

    // 初期化
    virtual void Initialize() override = 0;

    // 更新
    virtual void Update() override = 0;

    // 描画
    virtual void Draw() override = 0;

    // 終了処理
    virtual void Finalize() override = 0;
};
