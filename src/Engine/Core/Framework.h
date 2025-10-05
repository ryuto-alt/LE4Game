#pragma once

#include "WinApp.h"

// ゲームフレームワークの基底クラス
// 汎用的な部分を定義し、派生クラスで具体的な実装を行う
class Framework {
public: // メンバ関数
    // 仮想デストラクタ
    virtual ~Framework() = default;

    // 初期化
    virtual void Initialize() = 0;

    // 終了
    virtual void Finalize() = 0;

    // 毎フレーム更新
    virtual void Update() = 0;

    // 描画
    virtual void Draw() = 0;

    // 終了チェック
    virtual bool IsEndRequest() { return endRequest_; }

    // 実行（ゲームループの実行）
    void Run();

protected:
    // 終了フラグ
    bool endRequest_ = false;
};