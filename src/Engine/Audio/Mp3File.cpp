// src/Engine/Audio/Mp3File.cpp
#include "Mp3File.h"
#include <cassert>

Mp3File::Mp3File() {
    // WAVEフォーマットの初期化
    ZeroMemory(&waveFormat, sizeof(WAVEFORMATEX));
}

Mp3File::~Mp3File() {
    // 何もする必要なし
}

bool Mp3File::Load(const std::string& filePath) {
    // Media Foundationオブジェクト
    Microsoft::WRL::ComPtr<IMFSourceReader> sourceReader;
    Microsoft::WRL::ComPtr<IMFMediaType> mediaType;

    // ファイルパスをワイド文字列に変換
    wchar_t wFilePath[MAX_PATH];
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wFilePath, filePath.c_str(), MAX_PATH);

    // ソースリーダーの作成
    HRESULT hr = MFCreateSourceReaderFromURL(wFilePath, nullptr, sourceReader.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // メディアタイプの取得
    hr = sourceReader->GetNativeMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        0,
        mediaType.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // PCMフォーマットに変換
    hr = MFCreateMediaType(mediaType.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) {
        return false;
    }

    hr = mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr)) {
        return false;
    }

    // メディアタイプの設定
    hr = sourceReader->SetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        nullptr,
        mediaType.Get());
    if (FAILED(hr)) {
        return false;
    }

    // 変換後のメディアタイプを取得
    hr = sourceReader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM,
        mediaType.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // WAVEフォーマット情報を取得
    UINT32 formatSize = 0;
    WAVEFORMATEX* pWaveFormat = nullptr;
    hr = MFCreateWaveFormatExFromMFMediaType(
        mediaType.Get(),
        &pWaveFormat,
        &formatSize);
    if (FAILED(hr)) {
        return false;
    }

    // WAVEフォーマットをコピー
    memcpy(&waveFormat, pWaveFormat, sizeof(WAVEFORMATEX));
    CoTaskMemFree(pWaveFormat);

    // オーディオデータの読み込み
    audioData.clear();

    while (true) {
        // サンプルの読み込み
        Microsoft::WRL::ComPtr<IMFSample> sample;
        DWORD streamFlags = 0;

        hr = sourceReader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            nullptr,
            &streamFlags,
            nullptr,
            sample.GetAddressOf());

        if (FAILED(hr) || (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            break;
        }

        if (sample == nullptr) {
            continue;
        }

        // サンプルからメディアバッファを取得
        Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
        hr = sample->ConvertToContiguousBuffer(mediaBuffer.GetAddressOf());
        if (FAILED(hr)) {
            continue;
        }

        // メディアバッファからデータを取得
        BYTE* audioBuffer = nullptr;
        DWORD bufferSize = 0;

        hr = mediaBuffer->Lock(&audioBuffer, nullptr, &bufferSize);
        if (FAILED(hr)) {
            continue;
        }

        // データをコピー
        size_t offset = audioData.size();
        audioData.resize(offset + bufferSize);
        memcpy(audioData.data() + offset, audioBuffer, bufferSize);

        // バッファのロックを解除
        mediaBuffer->Unlock();
    }

    return !audioData.empty();
}

const WAVEFORMATEX& Mp3File::GetWaveFormat() const {
    return waveFormat;
}

const std::vector<BYTE>& Mp3File::GetAudioData() const {
    return audioData;
}