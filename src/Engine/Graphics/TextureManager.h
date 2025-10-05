#pragma once

#include <string>
#include "DirectXTex.h"
#include "d3dx12.h"
#include "DirectXCommon.h"
#include <unordered_map>

class SrvManager;

class TextureManager
{
private:
    static TextureManager* instance;

    TextureManager() = default;
    ~TextureManager(); // デストラクタを明示的に実装
    TextureManager(TextureManager&) = default;
    TextureManager& operator=(TextureManager&) = delete;

    // テクスチャ1枚分のデータ
    struct TextureData {
        std::string filePath;
        DirectX::TexMetadata metadata;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
        uint32_t srvIndex;
    };
public:
    // シングルトンインスタンス
    static TextureManager* GetInstance();

    // 終了
    void Finalize();

    // 初期化
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // メタデータを取得
    const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

    // テクスチャファイルの読み込み
    // bool型の戻り値に変更（成功/失敗を返すため）
    bool LoadTexture(const std::string& filePath);

    // テクスチャ番号からCPUハンドルを取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

    // テクスチャのSRVインデックスを取得（追加）
    uint32_t GetSrvIndex(const std::string& filePath);

    // テクスチャが存在するかチェック（新規追加）
    bool IsTextureExists(const std::string& filePath) const {
        return textureDatas.find(filePath) != textureDatas.end();
    }

    // デフォルトテクスチャを読み込む（新規追加）
    void LoadDefaultTexture();

    // デフォルトテクスチャのパスを取得（新規追加）
    const std::string& GetDefaultTexturePath() const {
        static const std::string defaultTexturePath = "Resources/textures/default_white.png";
        return defaultTexturePath;
    }
    
    // SRVディスクリプタヒープを取得（D3D12描画用）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSrvDescriptorHeap() const;

    // バッチ処理の開始
    void BeginBatch();
    
    // バッチ処理の終了（ここでCommandKickを実行）
    void EndBatch();
    
    // バッチ処理用のテクスチャ読み込み（CommandKickを遅延）
    bool LoadTextureDeferred(const std::string& filePath);

private:
    // テクスチャデータ
    std::unordered_map<std::string, TextureData> textureDatas;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
};