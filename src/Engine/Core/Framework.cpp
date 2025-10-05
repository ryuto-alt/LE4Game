#include "Framework.h"

void Framework::Run() {
    // ゲームの初期化
    Initialize();

    // ゲームループ
    while (true) {
        // 毎フレーム更新
        Update();

        // 終了リクエストがあったら抜ける
        if (IsEndRequest()) {
            break;
        }

        // 描画
        Draw();
    }

    // ゲーム終了
    Finalize();
}