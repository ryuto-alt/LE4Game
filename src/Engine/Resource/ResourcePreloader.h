#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include "AnimatedModel.h"

class DirectXCommon;

// リソースのプリロード管理クラス
class ResourcePreloader {
public:
    // シングルトンインスタンスの取得
    static ResourcePreloader* GetInstance();

    // アニメーションモデルのプリロード（優先度付き）
    void PreloadAnimatedModel(const std::string& key, const std::string& directoryPath, const std::string& filename, DirectXCommon* dxCommon, bool highPriority = true);

    // 軽量モード用の高速プリロード
    void PreloadAnimatedModelLightweight(const std::string& key, const std::string& directoryPath, const std::string& filename, DirectXCommon* dxCommon);

    // プリロードされたモデルの取得（所有権を移動）
    std::unique_ptr<AnimatedModel> GetPreloadedModel(const std::string& key);

    // プリロードされたモデルが存在するか確認
    bool HasPreloadedModel(const std::string& key) const;

    // プリロード進行状況を取得（0.0-1.0）
    float GetPreloadProgress() const { return totalCount_ > 0 ? float(loadedCount_) / float(totalCount_) : 1.0f; }

    // すべてのプリロードされたリソースをクリア
    void ClearAll();

private:
    ResourcePreloader() = default;
    ~ResourcePreloader() = default;
    ResourcePreloader(const ResourcePreloader&) = delete;
    ResourcePreloader& operator=(const ResourcePreloader&) = delete;

    // プリロードされたモデルの保存
    std::unordered_map<std::string, std::unique_ptr<AnimatedModel>> preloadedModels_;
    
    // プリロード進行状況管理
    int totalCount_ = 0;
    int loadedCount_ = 0;
};