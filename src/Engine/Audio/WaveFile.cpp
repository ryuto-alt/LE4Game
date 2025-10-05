// src/Engine/Audio/WaveFile.cpp
#include "WaveFile.h"
#include <cassert>
#include <Windows.h>

// WAVファイルのヘッダー構造
#pragma pack(push, 1)
struct WAVHeader {
    // RIFFヘッダー
    char riffId[4];        // "RIFF"
    uint32_t fileSize;     // ファイルサイズ - 8
    char waveId[4];        // "WAVE"

    // fmtチャンク
    char fmtId[4];         // "fmt "
    uint32_t fmtSize;      // fmtチャンクサイズ（通常16）
    uint16_t audioFormat;  // フォーマット（1 = PCM）
    uint16_t numChannels;  // チャンネル数
    uint32_t sampleRate;   // サンプリングレート
    uint32_t byteRate;     // データ速度
    uint16_t blockAlign;   // ブロックサイズ
    uint16_t bitsPerSample;// サンプルあたりのビット数

    // dataチャンク（実際は可変位置だが、シンプルなWAVではここにある）
    char dataId[4];        // "data"
    uint32_t dataSize;     // データサイズ
};
#pragma pack(pop)

WaveFile::WaveFile() {
    // WAVEフォーマットの初期化
    ZeroMemory(&waveFormat, sizeof(WAVEFORMATEX));
}

WaveFile::~WaveFile() {
    // 何もする必要なし
}

bool WaveFile::Load(const std::string& filePath) {
    // ファイルの存在確認
    DWORD fileAttributes = GetFileAttributesA(filePath.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        OutputDebugStringA(("WaveFile: File not found - " + filePath + "\n").c_str());
        return false;
    }

    // ファイルを開く
    FILE* file = nullptr;
    errno_t err = fopen_s(&file, filePath.c_str(), "rb");
    if (err != 0 || !file) {
        OutputDebugStringA(("WaveFile: Failed to open file - " + filePath + "\n").c_str());
        return false;
    }

    // ファイルサイズの取得
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // ファイルサイズが最小サイズより小さい場合はエラー
    if (fileSize < sizeof(WAVHeader)) {
        OutputDebugStringA(("WaveFile: File too small - " + filePath + "\n").c_str());
        fclose(file);
        return false;
    }

    // 簡易的なWAVヘッダーの読み込み（標準的なWAVファイル向け）
    WAVHeader header;
    size_t readSize = fread(&header, sizeof(WAVHeader), 1, file);

    if (readSize != 1) {
        OutputDebugStringA(("WaveFile: Failed to read header - " + filePath + "\n").c_str());
        fclose(file);
        return false;
    }

    // "RIFF"チェック
    if (memcmp(header.riffId, "RIFF", 4) != 0) {
        OutputDebugStringA(("WaveFile: Not a RIFF file - " + filePath + "\n").c_str());
        fclose(file);
        return false;
    }

    // "WAVE"チェック
    if (memcmp(header.waveId, "WAVE", 4) != 0) {
        OutputDebugStringA(("WaveFile: Not a WAVE file - " + filePath + "\n").c_str());
        fclose(file);
        return false;
    }

    // "fmt "チェック
    if (memcmp(header.fmtId, "fmt ", 4) != 0) {
        OutputDebugStringA(("WaveFile: fmt chunk not found - " + filePath + "\n").c_str());
        fclose(file);
        return false;
    }

    // より複雑なWAVファイルの場合、ここでfmtチャンクの残りを読み飛ばす
    // 標準的なfmtチャンクは16バイトだが、拡張情報がある場合はそれ以上
    if (header.fmtSize > 16) {
        fseek(file, header.fmtSize - 16, SEEK_CUR);
    }

    // データチャンクを探す
    // 注: 簡易的なWAVファイルでは上記のヘッダー構造で十分ですが、
    // 実際のWAVファイルではdataチャンクの前に他のチャンクが存在する場合もあります

    // dataチャンクのIDが見つからない場合は探す
    if (memcmp(header.dataId, "data", 4) != 0) {
        OutputDebugStringA(("WaveFile: Searching for data chunk - " + filePath + "\n").c_str());

        char chunkId[4];
        uint32_t chunkSize;

        // ファイルの最後まで探す
        while (!feof(file)) {
            // チャンクIDとサイズを読む
            if (fread(chunkId, 1, 4, file) != 4 ||
                fread(&chunkSize, sizeof(uint32_t), 1, file) != 1) {
                break;
            }

            // dataチャンクを見つけた
            if (memcmp(chunkId, "data", 4) == 0) {
                header.dataSize = chunkSize;
                break;
            }

            // このチャンクをスキップ
            fseek(file, chunkSize, SEEK_CUR);
        }

        // dataチャンクが見つからなかった
        if (memcmp(chunkId, "data", 4) != 0) {
            OutputDebugStringA(("WaveFile: data chunk not found - " + filePath + "\n").c_str());
            fclose(file);
            return false;
        }
    }

    // WAVEフォーマットの設定
    waveFormat.wFormatTag = header.audioFormat;
    waveFormat.nChannels = header.numChannels;
    waveFormat.nSamplesPerSec = header.sampleRate;
    waveFormat.nAvgBytesPerSec = header.byteRate;
    waveFormat.nBlockAlign = header.blockAlign;
    waveFormat.wBitsPerSample = header.bitsPerSample;
    waveFormat.cbSize = 0;

    // オーディオデータの読み込み
    audioData.resize(header.dataSize);
    readSize = fread(audioData.data(), 1, header.dataSize, file);

    if (readSize != header.dataSize) {
        OutputDebugStringA(("WaveFile: Failed to read audio data - " + filePath +
            "\nExpected: " + std::to_string(header.dataSize) +
            " bytes, Read: " + std::to_string(readSize) + " bytes\n").c_str());
        fclose(file);
        return false;
    }

    // ファイルを閉じる
    fclose(file);

    // 成功
    OutputDebugStringA(("WaveFile: Successfully loaded - " + filePath +
        "\nChannels: " + std::to_string(waveFormat.nChannels) +
        ", Sample Rate: " + std::to_string(waveFormat.nSamplesPerSec) +
        ", Bits: " + std::to_string(waveFormat.wBitsPerSample) +
        ", Data Size: " + std::to_string(audioData.size()) + " bytes\n").c_str());
    return true;
}

const WAVEFORMATEX& WaveFile::GetWaveFormat() const {
    return waveFormat;
}

const std::vector<BYTE>& WaveFile::GetAudioData() const {
    return audioData;
}