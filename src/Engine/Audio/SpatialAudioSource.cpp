#include "SpatialAudioSource.h"
#include "Mymath.h"
#include <cmath>

SpatialAudioSource::SpatialAudioSource()
    : position_(Vector3{0.0f, 0.0f, 0.0f})
    , velocity_(Vector3{0.0f, 0.0f, 0.0f})
    , forward_(Vector3{0.0f, 0.0f, 1.0f})
    , up_(Vector3{0.0f, 1.0f, 0.0f})
    , baseVolume_(1.0f)
    , currentVolume_(1.0f)
    , maxDistance_(100.0f)
    , minDistance_(1.0f)
    , dopplerScale_(1.0f)
    , distanceToListener_(0.0f)
    , lastListenerForward_(Vector3{0.0f, 0.0f, 1.0f})
    , isInitialized_(false)
    , isPlaying_(false)
{
}

SpatialAudioSource::~SpatialAudioSource() {
    if (isPlaying_) {
        Stop();
    }
}

bool SpatialAudioSource::Initialize(const std::string& audioName, const Vector3& position) {
    audioName_ = audioName;
    position_ = position;
    isInitialized_ = true;
    
    return true;
}

void SpatialAudioSource::Update(const Vector3& listenerPosition, const Vector3& listenerForward) {
    if (!isInitialized_) return;

    // 3D音響計算
    Calculate3DAudio(listenerPosition, listenerForward);

    // AudioManagerに音量を適用
    if (isPlaying_) {
        // パンニング計算
        Vector3 directionToSource = {
            position_.x - listenerPosition.x,
            position_.y - listenerPosition.y,
            position_.z - listenerPosition.z
        };
        
        float leftVolume, rightVolume;
        CalculatePanning(directionToSource, lastListenerForward_, leftVolume, rightVolume);
        
        // 距離減衰を左右の音量に適用
        leftVolume *= currentVolume_;
        rightVolume *= currentVolume_;
        
        // AudioManagerに左右の音量を個別適用
        AudioManager::GetInstance()->SetLeftRightVolume(audioName_, leftVolume, rightVolume);
        
        // 全体音量も設定（互換性のため）
        float finalVolume = (leftVolume + rightVolume) * 0.5f;
        AudioManager::GetInstance()->SetVolume(audioName_, finalVolume);
    }
}

void SpatialAudioSource::Play(bool loop) {
    if (!isInitialized_) {
        return;
    }

    AudioManager::GetInstance()->Play(audioName_, loop);
    isPlaying_ = true;
}

void SpatialAudioSource::Stop() {
    if (!isInitialized_) return;

    AudioManager::GetInstance()->Stop(audioName_);
    isPlaying_ = false;
}

void SpatialAudioSource::Pause() {
    if (!isInitialized_) return;

    AudioManager::GetInstance()->Pause(audioName_);
}

void SpatialAudioSource::Resume() {
    if (!isInitialized_) return;

    AudioManager::GetInstance()->Resume(audioName_);
}

void SpatialAudioSource::SetPosition(const Vector3& position) {
    position_ = position;
}

void SpatialAudioSource::SetVelocity(const Vector3& velocity) {
    velocity_ = velocity;
}

void SpatialAudioSource::SetOrientation(const Vector3& forward, const Vector3& up) {
    forward_ = forward;
    up_ = up;
}

void SpatialAudioSource::SetVolume(float volume) {
    baseVolume_ = (volume < 0.0f) ? 0.0f : (volume > 1.0f) ? 1.0f : volume;
}

void SpatialAudioSource::SetMaxDistance(float distance) {
    maxDistance_ = (distance > minDistance_) ? distance : minDistance_;
}

void SpatialAudioSource::SetMinDistance(float distance) {
    minDistance_ = (distance > 0.1f) ? distance : 0.1f;
    maxDistance_ = (maxDistance_ > minDistance_) ? maxDistance_ : minDistance_;
}

void SpatialAudioSource::SetDopplerScale(float scale) {
    dopplerScale_ = (scale < 0.0f) ? 0.0f : (scale > 10.0f) ? 10.0f : scale;
}

bool SpatialAudioSource::IsPlaying() const {
    if (!isInitialized_) return false;
    return AudioManager::GetInstance()->IsPlaying(audioName_);
}

void SpatialAudioSource::Calculate3DAudio(const Vector3& listenerPosition, const Vector3& listenerForward) {
    // リスナーの前方向を保存
    lastListenerForward_ = listenerForward;
    
    // リスナーからエミッタへのベクトル
    Vector3 directionToSource = {
        position_.x - listenerPosition.x,
        position_.y - listenerPosition.y,
        position_.z - listenerPosition.z
    };

    // 距離計算
    distanceToListener_ = sqrtf(
        directionToSource.x * directionToSource.x +
        directionToSource.y * directionToSource.y +
        directionToSource.z * directionToSource.z
    );

    // 距離による減衰計算
    float distanceAttenuation = CalculateDistanceAttenuation(distanceToListener_);

    // 最終音量を計算
    currentVolume_ = baseVolume_ * distanceAttenuation;

    // 音が聞こえない距離の場合は音量を0に
    if (distanceToListener_ > maxDistance_) {
        currentVolume_ = 0.0f;
    }

    // 音量の範囲を制限
    currentVolume_ = (currentVolume_ < 0.0f) ? 0.0f : (currentVolume_ > 1.0f) ? 1.0f : currentVolume_;
}

float SpatialAudioSource::CalculateDistanceAttenuation(float distance) const {
    if (distance <= minDistance_) {
        return 1.0f; // 最小距離以下では減衰なし
    }
    
    if (distance >= maxDistance_) {
        return 0.0f; // 最大距離以上では無音
    }

    // 線形減衰
    float attenuation = 1.0f - ((distance - minDistance_) / (maxDistance_ - minDistance_));
    return (attenuation < 0.0f) ? 0.0f : (attenuation > 1.0f) ? 1.0f : attenuation;
}

void SpatialAudioSource::CalculatePanning(const Vector3& directionToSource, const Vector3& listenerForward, float& leftVolume, float& rightVolume) const {
    // 正規化
    float length = sqrtf(
        directionToSource.x * directionToSource.x +
        directionToSource.y * directionToSource.y +
        directionToSource.z * directionToSource.z
    );

    if (length > 0.0f) {
        Vector3 normalizedDirection = {
            directionToSource.x / length,
            directionToSource.y / length,
            directionToSource.z / length
        };

        // リスナーの右方向ベクトルを計算（左手座標系）
        Vector3 listenerRight = {
            listenerForward.z * 0.0f - listenerForward.y * 0.0f,  // Y成分は0と仮定
            listenerForward.x * 0.0f - listenerForward.z * 0.0f,  // 外積
            listenerForward.y * 0.0f - listenerForward.x * 0.0f
        };
        
        // 簡単な右方向ベクトル計算（Y=0と仮定）
        listenerRight.x = -listenerForward.z;
        listenerRight.y = 0.0f;
        listenerRight.z = listenerForward.x;
        
        // 正規化
        float rightLength = sqrtf(listenerRight.x * listenerRight.x + listenerRight.z * listenerRight.z);
        if (rightLength > 0.0f) {
            listenerRight.x /= rightLength;
            listenerRight.z /= rightLength;
        }

        // リスナーから見た相対的な左右位置を内積で計算
        float pan = normalizedDirection.x * listenerRight.x + normalizedDirection.z * listenerRight.z;
        
        // パンニングを反転（左右を正しくする）
        pan = -pan;

        // パンニングから左右の音量を計算
        float leftVol = 0.5f - pan * 0.5f;
        float rightVol = 0.5f + pan * 0.5f;
        leftVolume = (leftVol < 0.0f) ? 0.0f : (leftVol > 1.0f) ? 1.0f : leftVol;
        rightVolume = (rightVol < 0.0f) ? 0.0f : (rightVol > 1.0f) ? 1.0f : rightVol;
    } else {
        // 距離が0の場合は中央
        leftVolume = rightVolume = 0.5f;
    }
}