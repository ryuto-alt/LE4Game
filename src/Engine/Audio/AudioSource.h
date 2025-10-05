// src/Engine/Audio/AudioSource.h
#pragma once

#include <xaudio2.h>
#include <string>
#include <vector>
#include <memory>

// 前方宣言
class WaveFile;
class Mp3File;

// オーディオソースクラス
class AudioSource {
private:
    // ソースボイス
    IXAudio2SourceVoice* sourceVoice;

    // オーディオバッファ
    std::vector<BYTE> audioData;

    // オーディオフォーマット
    WAVEFORMATEX waveFormat;

    // 再生状態
    bool isPlaying;
    bool isPaused;
    bool isLooping;

    // ボリューム
    float volume;

public:
    // コンストラクタ
    AudioSource();

    // デストラクタ
    ~AudioSource();

    // WAVファイルからの初期化
    bool Initialize(IXAudio2* xAudio2, WaveFile* waveFile);

    // MP3ファイルからの初期化
    bool Initialize(IXAudio2* xAudio2, Mp3File* mp3File);

    // 再生
    void Play(bool looping = false);

    // 停止
    void Stop();

    // 一時停止
    void Pause();

    // 再開
    void Resume();

    // ボリューム設定（0.0f ～ 1.0f）
    void SetVolume(float volume);

    // パンニング設定（-1.0f（左）～ 1.0f（右））
    void SetPanning(float pan);

    // 左右の音量を個別設定
    void SetLeftRightVolume(float leftVolume, float rightVolume);

    // 再生中かどうか
    bool IsPlaying() const;

    // 一時停止中かどうか
    bool IsPaused() const;

    // ループ再生中かどうか
    bool IsLooping() const;

    // ソースボイスの取得
    IXAudio2SourceVoice* GetSourceVoice() const { return sourceVoice; }

    // コールバックハンドラクラス
    class VoiceCallback : public IXAudio2VoiceCallback {
    public:
        // 参照するAudioSource
        AudioSource* audioSource;

        // コンストラクタ
        VoiceCallback() : audioSource(nullptr) {}

        // 再生終了時のコールバック
        void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) override;

        // 未使用のコールバックは空実装
        void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 BytesRequired) override {}
        void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
        void STDMETHODCALLTYPE OnStreamEnd() override {}
        void STDMETHODCALLTYPE OnBufferStart(void* pBufferContext) override {}
        void STDMETHODCALLTYPE OnLoopEnd(void* pBufferContext) override {}
        void STDMETHODCALLTYPE OnVoiceError(void* pBufferContext, HRESULT Error) override {}
    };

    // コールバックハンドラ
    VoiceCallback voiceCallback;
};