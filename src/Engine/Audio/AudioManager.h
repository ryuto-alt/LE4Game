// src/Engine/Audio/AudioManager.h
#pragma once

#include <xaudio2.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl.h>
#include "AudioSource.h"

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")

class AudioManager {
private:
    // シングルトンインスタンス
    static AudioManager* instance;

    // XAudio2インターフェイス
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;

    // マスターボイス
    IXAudio2MasteringVoice* masteringVoice;

    // オーディオソースのマップ
    std::unordered_map<std::string, std::unique_ptr<AudioSource>> audioSources;

    // 再生中のソースリスト
    std::vector<AudioSource*> playingSources;

    // コンストラクタ（シングルトン）
    AudioManager();

    // デストラクタ
    ~AudioManager();

    // コピー禁止
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // Media Foundation初期化フラグ
    bool mfInitialized;

public:
    // シングルトンインスタンスの取得
    static AudioManager* GetInstance();

    // 初期化
    void Initialize();

    // 終了処理
    void Finalize();
    
    // シングルトンインスタンスの破棄（メモリリーク対策）
    static void DestroyInstance();

    // 更新処理
    void Update();

    // WAVファイルの読み込み
    bool LoadWAV(const std::string& name, const std::string& filePath);

    // MP3ファイルの読み込み
    bool LoadMP3(const std::string& name, const std::string& filePath);

    // 音声の再生
    void Play(const std::string& name, bool looping = false);

    // 音声の停止
    void Stop(const std::string& name);

    // 音声の一時停止
    void Pause(const std::string& name);

    // 音声の再開
    void Resume(const std::string& name);

    // ボリューム設定（0.0f ～ 1.0f）
    void SetVolume(const std::string& name, float volume);

    // パンニング設定（-1.0f（左）～ 1.0f（右））
    void SetPanning(const std::string& name, float pan);

    // 左右の音量を個別設定
    void SetLeftRightVolume(const std::string& name, float leftVolume, float rightVolume);

    // マスターボリューム設定（0.0f ～ 1.0f）
    void SetMasterVolume(float volume);

    // 再生中かどうか
    bool IsPlaying(const std::string& name);

    // オーディオソースの取得
    AudioSource* GetAudioSource(const std::string& name);

    // XAudio2インターフェイスの取得
    IXAudio2* GetXAudio2() { return xAudio2.Get(); }
};