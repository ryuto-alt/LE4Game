// src/Engine/Audio/AudioSource.cpp
#include "AudioSource.h"
#include "WaveFile.h"
#include "Mp3File.h"
#include <cassert>

AudioSource::AudioSource()
    : sourceVoice(nullptr), isPlaying(false), isPaused(false), isLooping(false), volume(1.0f) {
    // コールバックハンドラの設定
    voiceCallback.audioSource = this;
}

AudioSource::~AudioSource() {
    // 再生中なら停止
    if (sourceVoice) {
        sourceVoice->Stop();
        sourceVoice->FlushSourceBuffers();
        sourceVoice->DestroyVoice();
        sourceVoice = nullptr;
    }
}

bool AudioSource::Initialize(IXAudio2* xAudio2, WaveFile* waveFile) {
    assert(xAudio2);
    assert(waveFile);

    // WAVEフォーマットを取得
    waveFormat = waveFile->GetWaveFormat();

    // オーディオデータを取得
    audioData = waveFile->GetAudioData();

    // ソースボイスの作成
    HRESULT hr = xAudio2->CreateSourceVoice(
        &sourceVoice,
        &waveFormat,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        &voiceCallback);

    return SUCCEEDED(hr);
}

bool AudioSource::Initialize(IXAudio2* xAudio2, Mp3File* mp3File) {
    assert(xAudio2);
    assert(mp3File);

    // WAVEフォーマットを取得
    waveFormat = mp3File->GetWaveFormat();

    // オーディオデータを取得
    audioData = mp3File->GetAudioData();

    // ソースボイスの作成
    HRESULT hr = xAudio2->CreateSourceVoice(
        &sourceVoice,
        &waveFormat,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        &voiceCallback);

    return SUCCEEDED(hr);
}

void AudioSource::Play(bool looping) {
    if (!sourceVoice) {
        return;
    }

    // 既に再生中なら一度停止
    if (isPlaying) {
        sourceVoice->Stop();
        sourceVoice->FlushSourceBuffers();
    }

    // バッファの設定
    XAUDIO2_BUFFER buffer = {};
    buffer.AudioBytes = static_cast<UINT32>(audioData.size());
    buffer.pAudioData = audioData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    // ループ設定
    if (looping) {
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    // バッファの登録
    HRESULT hr = sourceVoice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr)) {
        return;
    }

    // ボリュームの設定
    sourceVoice->SetVolume(volume);

    // 再生開始
    sourceVoice->Start();

    // 状態の更新
    isPlaying = true;
    isPaused = false;
    isLooping = looping;
}

void AudioSource::Stop() {
    if (!sourceVoice || !isPlaying) {
        return;
    }

    // 停止
    sourceVoice->Stop();
    sourceVoice->FlushSourceBuffers();

    // 状態の更新
    isPlaying = false;
    isPaused = false;
}

void AudioSource::Pause() {
    if (!sourceVoice || !isPlaying || isPaused) {
        return;
    }

    // 一時停止
    sourceVoice->Stop();

    // 状態の更新
    isPaused = true;
}

void AudioSource::Resume() {
    if (!sourceVoice || !isPlaying || !isPaused) {
        return;
    }

    // 再開
    sourceVoice->Start();

    // 状態の更新
    isPaused = false;
}

void AudioSource::SetVolume(float volume) {
    // 0.0f～1.0fの範囲に制限
    this->volume = (volume < 0.0f) ? 0.0f : (volume > 1.0f) ? 1.0f : volume;

    if (sourceVoice) {
        sourceVoice->SetVolume(this->volume);
    }
}

void AudioSource::SetPanning(float pan) {
    if (!sourceVoice) {
        return;
    }

    // パンニング設定（-1.0f（左）～ 1.0f（右））
    // XAudio2では出力マトリックスを使用してパンニングを実現
    // ステレオ出力の場合、2x2マトリックス
    float outputMatrix[4];
    
    // パンの値を制限
    pan = (pan < -1.0f) ? -1.0f : (pan > 1.0f) ? 1.0f : pan;
    
    // 左チャンネル（入力）から左スピーカー（出力）への音量
    outputMatrix[0] = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
    // 左チャンネル（入力）から右スピーカー（出力）への音量
    outputMatrix[1] = 0.0f;
    // 右チャンネル（入力）から左スピーカー（出力）への音量
    outputMatrix[2] = 0.0f;
    // 右チャンネル（入力）から右スピーカー（出力）への音量
    outputMatrix[3] = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
    
    // モノラル音源の場合の調整
    if (waveFormat.nChannels == 1) {
        outputMatrix[1] = (pan >= 0.0f) ? pan : 0.0f;
        outputMatrix[2] = (pan <= 0.0f) ? -pan : 0.0f;
        outputMatrix[3] = outputMatrix[0];
    }
    
    sourceVoice->SetOutputMatrix(nullptr, waveFormat.nChannels, 2, outputMatrix);
}

void AudioSource::SetLeftRightVolume(float leftVolume, float rightVolume) {
    if (!sourceVoice) {
        return;
    }

    // 音量の制限
    leftVolume = (leftVolume < 0.0f) ? 0.0f : (leftVolume > 1.0f) ? 1.0f : leftVolume;
    rightVolume = (rightVolume < 0.0f) ? 0.0f : (rightVolume > 1.0f) ? 1.0f : rightVolume;
    
    // 出力マトリックス設定
    float outputMatrix[4];
    
    if (waveFormat.nChannels == 1) {
        // モノラル音源の場合
        outputMatrix[0] = leftVolume;   // モノラル入力 -> 左出力
        outputMatrix[1] = rightVolume;  // モノラル入力 -> 右出力
        sourceVoice->SetOutputMatrix(nullptr, 1, 2, outputMatrix);
    } else {
        // ステレオ音源の場合
        outputMatrix[0] = leftVolume;   // 左入力 -> 左出力
        outputMatrix[1] = 0.0f;         // 左入力 -> 右出力
        outputMatrix[2] = 0.0f;         // 右入力 -> 左出力
        outputMatrix[3] = rightVolume;  // 右入力 -> 右出力
        sourceVoice->SetOutputMatrix(nullptr, 2, 2, outputMatrix);
    }
}

bool AudioSource::IsPlaying() const {
    return isPlaying;
}

bool AudioSource::IsPaused() const {
    return isPaused;
}

bool AudioSource::IsLooping() const {
    return isLooping;
}

void AudioSource::VoiceCallback::OnBufferEnd(void* pBufferContext) {
    // ループ再生でない場合は再生終了
    if (audioSource && !audioSource->isLooping) {
        audioSource->isPlaying = false;
    }
}