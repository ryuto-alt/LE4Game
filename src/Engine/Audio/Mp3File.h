// src/Engine/Audio/Mp3File.h
#pragma once

#include <string>
#include <vector>
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")

// MP3ファイルクラス
class Mp3File {
private:
    // WAVEフォーマット
    WAVEFORMATEX waveFormat;

    // オーディオデータ
    std::vector<BYTE> audioData;

public:
    // コンストラクタ
    Mp3File();

    // デストラクタ
    ~Mp3File();

    // MP3ファイルの読み込み
    bool Load(const std::string& filePath);

    // WAVEフォーマットの取得
    const WAVEFORMATEX& GetWaveFormat() const;

    // オーディオデータの取得
    const std::vector<BYTE>& GetAudioData() const;
};