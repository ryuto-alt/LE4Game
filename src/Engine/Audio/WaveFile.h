// src/Engine/Audio/WaveFile.h
#pragma once

#include <string>
#include <vector>
#include <xaudio2.h>

// WAVファイルクラス
class WaveFile {
private:
    // WAVEフォーマット 
    WAVEFORMATEX waveFormat;

    // オーディオデータ
    std::vector<BYTE> audioData;

    // RIFFチャンクの読み込み
    bool ReadRIFFChunk(FILE* file);

    // フォーマットチャンクの読み込み
    bool ReadFormatChunk(FILE* file);

    // データチャンクの読み込み
    bool ReadDataChunk(FILE* file);

public:
    // コンストラクタ
    WaveFile();

    // デストラクタ
    ~WaveFile();

    // WAVファイルの読み込み
    bool Load(const std::string& filePath);

    // WAVEフォーマットの取得
    const WAVEFORMATEX& GetWaveFormat() const;

    // オーディオデータの取得
    const std::vector<BYTE>& GetAudioData() const;
};