#pragma once

#include "Mymath.h"
#include "AudioManager.h"
#include <memory>
#include <string>

// 3D空間オーディオソースクラス
class SpatialAudioSource {
public:
    // コンストラクタ
    SpatialAudioSource();
    ~SpatialAudioSource();

    // 初期化
    bool Initialize(const std::string& audioName, const Vector3& position);

    // 更新（リスナーとの相対位置を計算）
    void Update(const Vector3& listenerPosition, const Vector3& listenerForward);

    // 再生制御
    void Play(bool loop = false);
    void Stop();
    void Pause();
    void Resume();

    // 位置・向きの設定
    void SetPosition(const Vector3& position);
    void SetVelocity(const Vector3& velocity);
    void SetOrientation(const Vector3& forward, const Vector3& up = Vector3{0.0f, 1.0f, 0.0f});

    // 音響プロパティ
    void SetVolume(float volume);           // 基本音量
    void SetMaxDistance(float distance);    // 最大聞こえる距離
    void SetMinDistance(float distance);    // 減衰開始距離
    void SetDopplerScale(float scale);      // ドップラー効果の強さ

    // 状態取得
    bool IsPlaying() const;
    const Vector3& GetPosition() const { return position_; }
    float GetDistanceToListener() const { return distanceToListener_; }

private:
    // 3D音響計算
    void Calculate3DAudio(const Vector3& listenerPosition, const Vector3& listenerForward);
    
    // 距離減衰計算
    float CalculateDistanceAttenuation(float distance) const;
    
    // パンニング計算（左右の音の配分）
    void CalculatePanning(const Vector3& directionToSource, const Vector3& listenerForward, float& leftVolume, float& rightVolume) const;

private:
    std::string audioName_;                 // 使用するオーディオファイル名
    Vector3 position_;                      // エミッタの位置
    Vector3 velocity_;                      // エミッタの速度（ドップラー効果用）
    Vector3 forward_;                       // エミッタの向き
    Vector3 up_;                           // エミッタの上方向

    float baseVolume_;                      // 基本音量
    float currentVolume_;                   // 計算後の実際の音量
    float maxDistance_;                     // 最大聞こえる距離
    float minDistance_;                     // 減衰開始距離
    float dopplerScale_;                    // ドップラー効果スケール

    float distanceToListener_;              // リスナーまでの距離
    Vector3 lastListenerForward_;           // 最後に更新されたリスナーの前方向
    bool isInitialized_;                    // 初期化済みフラグ
    bool isPlaying_;                        // 再生状態
};