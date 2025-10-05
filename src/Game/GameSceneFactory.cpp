#include "GameSceneFactory.h"
#include "scene/TitleScene.h"      // タイトルシーン
#include "scene/GamePlayScene.h"   // 追加：ゲームプレイシーン
#include <stdexcept>

std::unique_ptr<IScene> GameSceneFactory::CreateScene(const std::string& sceneName) {
    // シーン名によって適切なシーンオブジェクトを生成
    if (sceneName == "Title") {
        return std::make_unique<TitleScene>();
    }
    else if (sceneName == "GamePlay") {
        return std::make_unique<GamePlayScene>();
    }
    // 他のシーンを追加する場合はここに追加

    // 該当するシーンがない場合は例外をスロー
    throw std::runtime_error("Unknown scene name: " + sceneName);
}
