#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <string>
#include <memory>
#include <chrono>
#include "Mymath.h"

class DirectXCommon;
class SrvManager;

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    // 初期化
    bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // 動画ファイルを読み込む
    bool LoadVideo(const std::string& filePath);

    // 更新（次のフレームを取得）
    void Update();

    // テクスチャを取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle() const { return srvHandleGPU_; }

    // 再生制御
    void Play();
    void Pause();
    void Stop();
    void SetLoop(bool loop) { isLoop_ = loop; }

    // 状態取得
    bool IsPlaying() const { return isPlaying_; }
    bool IsFinished() const { return isFinished_; }
    Vector2 GetVideoSize() const { return videoSize_; }

private:
    // Media Foundationの初期化
    bool InitializeMediaFoundation();
    void FinalizeMediaFoundation();

    // フレームをテクスチャに変換
    bool UpdateTexture(IMFSample* sample);

    // テクスチャリソースを作成
    bool CreateTextureResource();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // Media Foundation
    Microsoft::WRL::ComPtr<IMFSourceReader> sourceReader_;
    DWORD videoStreamIndex_ = 0;

    // テクスチャリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer_;
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_;
    uint32_t srvIndex_ = 0;

    // 動画情報
    Vector2 videoSize_ = { 1920.0f, 1080.0f };
    uint32_t videoWidth_ = 1920;
    uint32_t videoHeight_ = 1080;

    // 再生状態
    bool isPlaying_ = false;
    bool isPaused_ = false;
    bool isFinished_ = false;
    bool isLoop_ = false;

    // タイミング制御
    LONGLONG videoDuration_ = 0;
    LONGLONG currentTime_ = 0;
    double frameRate_ = 30.0;
    double frameTime_ = 1.0 / 30.0;
    double elapsedTime_ = 0.0;
    std::chrono::high_resolution_clock::time_point lastTime_;

    // Media Foundation初期化フラグ
    static bool isMFInitialized_;
    static int mfRefCount_;

    // 初回フレーム取得フラグ
    bool isFirstFrame_ = true;
};
