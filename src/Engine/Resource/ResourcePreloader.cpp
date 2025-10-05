#include "ResourcePreloader.h"
#include "DirectXCommon.h"
#include <Windows.h>

ResourcePreloader* ResourcePreloader::GetInstance() {
    static ResourcePreloader instance;
    return &instance;
}

void ResourcePreloader::PreloadAnimatedModel(const std::string& key, const std::string& directoryPath, const std::string& filename, DirectXCommon* dxCommon, bool highPriority) {
    OutputDebugStringA(("ResourcePreloader: Preloading model with key: " + key + " (Priority: " + (highPriority ? "HIGH" : "LOW") + ")\n").c_str());
    
    // 既に存在する場合は何もしない
    if (preloadedModels_.find(key) != preloadedModels_.end()) {
        OutputDebugStringA(("ResourcePreloader: Model already preloaded with key: " + key + "\n").c_str());
        return;
    }
    
    totalCount_++;
    
    try {
        // モデルの作成と読み込み
        auto model = std::make_unique<AnimatedModel>();
        model->Initialize(dxCommon);
        model->LoadFromFile(directoryPath, filename);
        
        // 保存
        preloadedModels_[key] = std::move(model);
        loadedCount_++;
        
        OutputDebugStringA(("ResourcePreloader: Successfully preloaded model with key: " + key + " (" + std::to_string(loadedCount_) + "/" + std::to_string(totalCount_) + ")\n").c_str());
    } catch (const std::exception& e) {
        OutputDebugStringA(("ResourcePreloader: Failed to preload model " + key + ": " + e.what() + "\n").c_str());
    }
}

void ResourcePreloader::PreloadAnimatedModelLightweight(const std::string& key, const std::string& directoryPath, const std::string& filename, DirectXCommon* dxCommon) {
    OutputDebugStringA(("ResourcePreloader: Lightweight preloading model with key: " + key + "\n").c_str());
    
    // 既に存在する場合は何もしない
    if (preloadedModels_.find(key) != preloadedModels_.end()) {
        OutputDebugStringA(("ResourcePreloader: Model already preloaded with key: " + key + "\n").c_str());
        return;
    }
    
    totalCount_++;
    
    try {
        // 軽量読み込み：最低限の設定でモデル作成
        auto model = std::make_unique<AnimatedModel>();
        model->Initialize(dxCommon);
        
        // ファイル存在確認のみ行い、実際の読み込みは遅延
        DWORD fileAttributes = GetFileAttributesA((directoryPath + "/" + filename).c_str());
        if (fileAttributes != INVALID_FILE_ATTRIBUTES) {
            model->LoadFromFile(directoryPath, filename);
        }
        
        preloadedModels_[key] = std::move(model);
        loadedCount_++;
        
        OutputDebugStringA(("ResourcePreloader: Lightweight preload completed for " + key + "\n").c_str());
    } catch (const std::exception& e) {
        OutputDebugStringA(("ResourcePreloader: Lightweight preload failed for " + key + ": " + e.what() + "\n").c_str());
    }
}

std::unique_ptr<AnimatedModel> ResourcePreloader::GetPreloadedModel(const std::string& key) {
    auto it = preloadedModels_.find(key);
    if (it != preloadedModels_.end()) {
        OutputDebugStringA(("ResourcePreloader: Moving preloaded model with key: " + key + "\n").c_str());
        return std::move(it->second);
    }
    
    OutputDebugStringA(("ResourcePreloader: No preloaded model found with key: " + key + "\n").c_str());
    return nullptr;
}

bool ResourcePreloader::HasPreloadedModel(const std::string& key) const {
    return preloadedModels_.find(key) != preloadedModels_.end();
}

void ResourcePreloader::ClearAll() {
    OutputDebugStringA("ResourcePreloader: Clearing all preloaded resources\n");
    preloadedModels_.clear();
    totalCount_ = 0;
    loadedCount_ = 0;
}