// src/Engine/Audio/AudioManager.cpp
#include "AudioManager.h"
#include "WaveFile.h"
#include "Mp3File.h"
#include <cassert>

// 静的メンバの実体化
AudioManager* AudioManager::instance = nullptr;

AudioManager* AudioManager::GetInstance() {
    if (!instance) {
        instance = new AudioManager();
    }
    return instance;
}

AudioManager::AudioManager()
    : masteringVoice(nullptr), mfInitialized(false) {
}

AudioManager::~AudioManager() {
    Finalize();
}

void AudioManager::Initialize() {
    // XAudio2の初期化
    UINT32 flags = 0;
#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    HRESULT hr = XAudio2Create(xAudio2.GetAddressOf(), flags);
    assert(SUCCEEDED(hr));

    // マスターボイスの作成
    hr = xAudio2->CreateMasteringVoice(&masteringVoice);
    assert(SUCCEEDED(hr));

    // Media Foundationの初期化
    hr = MFStartup(MF_VERSION);
    if (SUCCEEDED(hr)) {
        mfInitialized = true;
    }
    else {
        // MP3やAACなどの読み込みはできないが、WAVは読み込める
        mfInitialized = false;
    }
}

void AudioManager::Finalize() {
    // 全てのオーディオソースを停止
    for (auto& pair : audioSources) {
        pair.second->Stop();
    }

    // オーディオソースをクリア
    audioSources.clear();
    playingSources.clear();

    // マスターボイスの解放
    if (masteringVoice) {
        masteringVoice->DestroyVoice();
        masteringVoice = nullptr;
    }

    // Media Foundationの終了処理
    if (mfInitialized) {
        MFShutdown();
        mfInitialized = false;
    }

    // XAudio2インターフェイスの解放は自動で行われる（ComPtr）
}

void AudioManager::DestroyInstance() {
    if (instance) {
        instance->Finalize();
        delete instance;
        instance = nullptr;
    }
}

void AudioManager::Update() {
    // 再生が終了したソースを検出して再生リストから削除
    auto it = playingSources.begin();
    while (it != playingSources.end()) {
        if (!(*it)->IsPlaying()) {
            it = playingSources.erase(it);
        }
        else {
            ++it;
        }
    }
}

bool AudioManager::LoadWAV(const std::string& name, const std::string& filePath) {
    // 既に読み込み済みかチェック
    if (audioSources.find(name) != audioSources.end()) {
        return true; // 既に読み込み済み
    }

    // ファイルの存在確認
    DWORD fileAttributes = GetFileAttributesA(filePath.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        OutputDebugStringA(("AudioManager: WAV file not found - " + filePath + "\n").c_str());
        return false;
    }

    OutputDebugStringA(("AudioManager: Loading WAV file - " + filePath + "\n").c_str());

    // 方法1: 従来のWaveFile方式を試す
    std::unique_ptr<WaveFile> waveFile = std::make_unique<WaveFile>();
    bool success = waveFile->Load(filePath);

    // 方法1が失敗した場合は、方法2: Media Foundation方式を試す
    if (!success && mfInitialized) {
        OutputDebugStringA("AudioManager: Trying Media Foundation for WAV loading\n");
        std::unique_ptr<Mp3File> mediaFile = std::make_unique<Mp3File>();
        success = mediaFile->Load(filePath);

        if (success) {
            // Audio Sourceの作成
            std::unique_ptr<AudioSource> audioSource = std::make_unique<AudioSource>();

            // MP3/WAVデータをAudioSourceに設定
            if (!audioSource->Initialize(xAudio2.Get(), mediaFile.get())) {
                OutputDebugStringA("AudioManager: Failed to initialize audio source\n");
                return false; // 初期化失敗
            }

            // マップに登録
            audioSources[name] = std::move(audioSource);

            OutputDebugStringA(("AudioManager: Successfully loaded WAV using Media Foundation - " + filePath + "\n").c_str());
            return true;
        }
    }

    // 従来のWaveFile方式が成功した場合
    if (success) {
        // AudioSourceの作成
        std::unique_ptr<AudioSource> audioSource = std::make_unique<AudioSource>();

        // WAVデータをAudioSourceに設定
        if (!audioSource->Initialize(xAudio2.Get(), waveFile.get())) {
            OutputDebugStringA("AudioManager: Failed to initialize audio source\n");
            return false; // 初期化失敗
        }

        // マップに登録
        audioSources[name] = std::move(audioSource);

        OutputDebugStringA(("AudioManager: Successfully loaded WAV - " + filePath + "\n").c_str());
        return true;
    }

    OutputDebugStringA(("AudioManager: Failed to load WAV - " + filePath + "\n").c_str());
    return false;
}

bool AudioManager::LoadMP3(const std::string& name, const std::string& filePath) {
    // Media Foundationが初期化されているかチェック
    if (!mfInitialized) {
        return false; // MP3はサポートされていない
    }

    // 既に読み込み済みかチェック
    if (audioSources.find(name) != audioSources.end()) {
        return true; // 既に読み込み済み
    }

    // Mp3Fileクラスを使用してMP3ファイルを読み込む
    std::unique_ptr<Mp3File> mp3File = std::make_unique<Mp3File>();
    if (!mp3File->Load(filePath)) {
        return false; // 読み込み失敗
    }

    // AudioSourceの作成
    std::unique_ptr<AudioSource> audioSource = std::make_unique<AudioSource>();

    // MP3データをAudioSourceに設定
    if (!audioSource->Initialize(xAudio2.Get(), mp3File.get())) {
        return false; // 初期化失敗
    }

    // マップに登録
    audioSources[name] = std::move(audioSource);

    return true;
}

void AudioManager::Play(const std::string& name, bool looping) {
    // 指定された名前のオーディオソースを検索
    auto it = audioSources.find(name);
    if (it == audioSources.end()) {
        return; // 見つからない
    }

    // 再生
    it->second->Play(looping);

    // 再生リストに追加（重複を避けるため、既に追加されていなければ）
    AudioSource* source = it->second.get();
    if (std::find(playingSources.begin(), playingSources.end(), source) == playingSources.end()) {
        playingSources.push_back(source);
    }
}

void AudioManager::Stop(const std::string& name) {
    // 指定された名前のオーディオソースを検索
    auto it = audioSources.find(name);
    if (it == audioSources.end()) {
        return; // 見つからない
    }

    // 停止
    it->second->Stop();
}

void AudioManager::Pause(const std::string& name) {
    // 指定された名前のオーディオソースを検索
    auto it = audioSources.find(name);
    if (it == audioSources.end()) {
        return; // 見つからない
    }

    // 一時停止
    it->second->Pause();
}

void AudioManager::Resume(const std::string& name) {
    // 指定された名前のオーディオソースを検索
    auto it = audioSources.find(name);
    if (it == audioSources.end()) {
        return; // 見つからない
    }

    // 再開
    it->second->Resume();
}

void AudioManager::SetVolume(const std::string& name, float volume) {
    // 指定された名前のオーディオソースを検索
    auto it = audioSources.find(name);
    if (it == audioSources.end()) {
        return; // 見つからない
    }

    // ボリューム設定
    it->second->SetVolume(volume);
}

void AudioManager::SetPanning(const std::string& name, float pan) {
    // 指定された名前のオーディオソースを検索
    auto it = audioSources.find(name);
    if (it == audioSources.end()) {
        return; // 見つからない
    }

    // パンニング設定（-1.0f（左）～ 1.0f（右））
    // 左右の音量を計算
    float leftVolume = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
    float rightVolume = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
    
    // AudioSourceにパンニングを適用（実装依存）
    it->second->SetPanning(pan);
}

void AudioManager::SetLeftRightVolume(const std::string& name, float leftVolume, float rightVolume) {
    // 指定された名前のオーディオソースを検索
    auto it = audioSources.find(name);
    if (it == audioSources.end()) {
        return; // 見つからない
    }

    // 左右の音量を個別設定
    it->second->SetLeftRightVolume(leftVolume, rightVolume);
}

void AudioManager::SetMasterVolume(float volume) {
    if (masteringVoice) {
        // マスターボリューム設定
        masteringVoice->SetVolume(volume);
    }
}

bool AudioManager::IsPlaying(const std::string& name) {
    // 指定された名前のオーディオソースを検索
    auto it = audioSources.find(name);
    if (it == audioSources.end()) {
        return false; // 見つからない
    }

    // 再生状態を返す
    return it->second->IsPlaying();
}

AudioSource* AudioManager::GetAudioSource(const std::string& name) {
    // 指定された名前のオーディオソースを検索
    auto it = audioSources.find(name);
    if (it == audioSources.end()) {
        return nullptr; // 見つからない
    }

    // オーディオソースを返す
    return it->second.get();
}