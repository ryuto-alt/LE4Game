# 3D空間オーディオ実装

## SpatialAudioListener

### C++
```cpp
// SpatialAudioListener.h
class SpatialAudioListener {
private:
    Vector3 position_;
    Vector3 velocity_;
    Vector3 forward_;
    Vector3 up_;
    float masterVolume_;
    float dopplerFactor_;
    float speedOfSound_;

public:
    SpatialAudioListener();

    void SetPosition(const Vector3& position);
    void SetVelocity(const Vector3& velocity);
    void SetOrientation(const Vector3& forward, const Vector3& up = Vector3{0.0f, 1.0f, 0.0f});

    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetForward() const { return forward_; }

    void SetMasterVolume(float volume);
    void SetDopplerFactor(float factor);
    void SetSpeedOfSound(float speed);
};

// SpatialAudioListener.cpp
SpatialAudioListener::SpatialAudioListener()
    : position_(Vector3{0.0f, 0.0f, 0.0f})
    , velocity_(Vector3{0.0f, 0.0f, 0.0f})
    , forward_(Vector3{0.0f, 0.0f, 1.0f})
    , up_(Vector3{0.0f, 1.0f, 0.0f})
    , masterVolume_(1.0f)
    , dopplerFactor_(1.0f)
    , speedOfSound_(343.0f)  // 空気中の音速（m/s）
{
}

void SpatialAudioListener::SetMasterVolume(float volume) {
    masterVolume_ = (volume < 0.0f) ? 0.0f : (volume > 1.0f) ? 1.0f : volume;
}

void SpatialAudioListener::SetDopplerFactor(float factor) {
    dopplerFactor_ = (factor < 0.0f) ? 0.0f : (factor > 10.0f) ? 10.0f : factor;
}

void SpatialAudioListener::SetSpeedOfSound(float speed) {
    speedOfSound_ = (speed > 1.0f) ? speed : 1.0f;
}
```

---

## SpatialAudioSource

### C++
```cpp
// SpatialAudioSource.h
class SpatialAudioSource {
private:
    std::string audioName_;
    Vector3 position_;
    Vector3 velocity_;
    Vector3 forward_;
    Vector3 up_;

    float baseVolume_;
    float currentVolume_;
    float maxDistance_;
    float minDistance_;
    float dopplerScale_;

    float distanceToListener_;
    Vector3 lastListenerForward_;
    bool isInitialized_;
    bool isPlaying_;

public:
    SpatialAudioSource();

    bool Initialize(const std::string& audioName, const Vector3& position);
    void Update(const Vector3& listenerPosition, const Vector3& listenerForward);

    void Play(bool loop = false);
    void Stop();

    void SetPosition(const Vector3& position);
    void SetVolume(float volume);
    void SetMaxDistance(float distance);
    void SetMinDistance(float distance);

    bool IsPlaying() const;

private:
    void Calculate3DAudio(const Vector3& listenerPosition, const Vector3& listenerForward);
    float CalculateDistanceAttenuation(float distance) const;
    void CalculatePanning(const Vector3& directionToSource, const Vector3& listenerForward,
                         float& leftVolume, float& rightVolume) const;
};

// SpatialAudioSource.cpp
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
    , isInitialized_(false)
    , isPlaying_(false)
{
}

bool SpatialAudioSource::Initialize(const std::string& audioName, const Vector3& position) {
    audioName_ = audioName;
    position_ = position;
    AudioManager::GetInstance()->LoadMP3(audioName_, audioName_);
    isInitialized_ = true;
    return true;
}

void SpatialAudioSource::Update(const Vector3& listenerPosition, const Vector3& listenerForward) {
    if (!isInitialized_) return;

    Calculate3DAudio(listenerPosition, listenerForward);

    Vector3 directionToSource = {
        position_.x - listenerPosition.x,
        position_.y - listenerPosition.y,
        position_.z - listenerPosition.z
    };

    float leftVolume, rightVolume;
    CalculatePanning(directionToSource, lastListenerForward_, leftVolume, rightVolume);

    leftVolume *= currentVolume_;
    rightVolume *= currentVolume_;

    AudioManager::GetInstance()->SetLeftRightVolume(audioName_, leftVolume, rightVolume);
}

void SpatialAudioSource::Calculate3DAudio(const Vector3& listenerPosition, const Vector3& listenerForward) {
    lastListenerForward_ = listenerForward;

    Vector3 directionToSource = {
        position_.x - listenerPosition.x,
        position_.y - listenerPosition.y,
        position_.z - listenerPosition.z
    };

    distanceToListener_ = sqrtf(
        directionToSource.x * directionToSource.x +
        directionToSource.y * directionToSource.y +
        directionToSource.z * directionToSource.z
    );

    float distanceAttenuation = CalculateDistanceAttenuation(distanceToListener_);
    currentVolume_ = baseVolume_ * distanceAttenuation;

    if (distanceToListener_ > maxDistance_) {
        currentVolume_ = 0.0f;
    }

    currentVolume_ = (currentVolume_ < 0.0f) ? 0.0f : (currentVolume_ > 1.0f) ? 1.0f : currentVolume_;
}

float SpatialAudioSource::CalculateDistanceAttenuation(float distance) const {
    if (distance <= minDistance_) {
        return 1.0f;
    }

    if (distance >= maxDistance_) {
        return 0.0f;
    }

    float attenuation = 1.0f - ((distance - minDistance_) / (maxDistance_ - minDistance_));
    return (attenuation < 0.0f) ? 0.0f : (attenuation > 1.0f) ? 1.0f : attenuation;
}

void SpatialAudioSource::CalculatePanning(const Vector3& directionToSource, const Vector3& listenerForward,
                                          float& leftVolume, float& rightVolume) const {
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

        // 右方向ベクトル計算（左手座標系）
        Vector3 listenerRight;
        listenerRight.x = -listenerForward.z;
        listenerRight.y = 0.0f;
        listenerRight.z = listenerForward.x;

        float rightLength = sqrtf(listenerRight.x * listenerRight.x + listenerRight.z * listenerRight.z);
        if (rightLength > 0.0f) {
            listenerRight.x /= rightLength;
            listenerRight.z /= rightLength;
        }

        // 相対的な左右位置を内積で計算
        float pan = normalizedDirection.x * listenerRight.x + normalizedDirection.z * listenerRight.z;
        pan = -pan;  // パンニング反転

        // 左右の音量計算
        float leftVol = 0.5f - pan * 0.5f;
        float rightVol = 0.5f + pan * 0.5f;
        leftVolume = (leftVol < 0.0f) ? 0.0f : (leftVol > 1.0f) ? 1.0f : leftVol;
        rightVolume = (rightVol < 0.0f) ? 0.0f : (rightVol > 1.0f) ? 1.0f : rightVol;
    } else {
        leftVolume = rightVolume = 0.5f;
    }
}
```

---

## AudioSource（XAudio2ベース）

### C++
```cpp
// AudioSource.h
class AudioSource {
private:
    IXAudio2SourceVoice* sourceVoice;
    std::vector<BYTE> audioData;
    WAVEFORMATEX waveFormat;
    bool isPlaying;
    bool isPaused;
    bool isLooping;
    float volume;

public:
    AudioSource();
    ~AudioSource();

    bool Initialize(IXAudio2* xAudio2, Mp3File* mp3File);

    void Play(bool looping = false);
    void Stop();
    void SetVolume(float volume);
    void SetLeftRightVolume(float leftVolume, float rightVolume);

    bool IsPlaying() const;
};

// AudioSource.cpp
void AudioSource::SetLeftRightVolume(float leftVolume, float rightVolume) {
    if (!sourceVoice) return;

    leftVolume = (leftVolume < 0.0f) ? 0.0f : (leftVolume > 1.0f) ? 1.0f : leftVolume;
    rightVolume = (rightVolume < 0.0f) ? 0.0f : (rightVolume > 1.0f) ? 1.0f : rightVolume;

    float outputMatrix[4];

    if (waveFormat.nChannels == 1) {
        // モノラル
        outputMatrix[0] = leftVolume;
        outputMatrix[1] = rightVolume;
        sourceVoice->SetOutputMatrix(nullptr, 1, 2, outputMatrix);
    } else {
        // ステレオ
        outputMatrix[0] = leftVolume;
        outputMatrix[1] = 0.0f;
        outputMatrix[2] = 0.0f;
        outputMatrix[3] = rightVolume;
        sourceVoice->SetOutputMatrix(nullptr, 2, 2, outputMatrix);
    }
}

void AudioSource::Play(bool looping) {
    if (!sourceVoice) return;

    if (isPlaying) {
        sourceVoice->Stop();
        sourceVoice->FlushSourceBuffers();
    }

    XAUDIO2_BUFFER buffer = {};
    buffer.AudioBytes = static_cast<UINT32>(audioData.size());
    buffer.pAudioData = audioData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    if (looping) {
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    sourceVoice->SubmitSourceBuffer(&buffer);
    sourceVoice->SetVolume(volume);
    sourceVoice->Start();

    isPlaying = true;
    isPaused = false;
    isLooping = looping;
}
```

---

## 使用例：Enemy

### C++
```cpp
// Enemy.cpp
void Enemy::UpdateDetectionSound() {
    if (!player_ || !audioListener_ || !detectionSound_) {
        return;
    }

    // リスナーの位置と向きを取得
    Vector3 listenerPos = audioListener_->GetPosition();
    Vector3 listenerForward = audioListener_->GetForward();

    // 3D位置を更新
    detectionSound_->SetPosition(position_);
    detectionSound_->Update(listenerPos, listenerForward);
}

void Enemy::UpdateFootstepAudio() {
    // リスナーの位置と向きを取得
    Vector3 listenerPos = audioListener_->GetPosition();
    Vector3 listenerForward = audioListener_->GetForward();

    // 交互に足音を再生
    if (useFootstep1_) {
        footstepSource1_->SetPosition(position_);
        footstepSource1_->Update(listenerPos, listenerForward);
        footstepSource1_->Play(false);
    } else {
        footstepSource2_->SetPosition(position_);
        footstepSource2_->Update(listenerPos, listenerForward);
        footstepSource2_->Play(false);
    }

    useFootstep1_ = !useFootstep1_;
}
```

---

### 距離減衰
- minDistance以下: 減衰なし（音量1.0）
- minDistance～maxDistance: 線形減衰
- maxDistance以上: 無音（音量0.0）

```cpp
float attenuation = 1.0f - ((distance - minDistance_) / (maxDistance_ - minDistance_));
```

### パンニング（左右の音配分）
- リスナーの右方向ベクトルを計算（左手座標系）
- 音源への方向ベクトルと内積でパンを算出
- pan = -1.0（左）～ 1.0（右）
- leftVolume = 0.5 - pan × 0.5
- rightVolume = 0.5 + pan × 0.5

### XAudio2出力マトリックス
モノラル音源（1ch → 2ch）:
```
[leftVolume, rightVolume]
```

ステレオ音源（2ch → 2ch）:
```
[leftVolume,  0.0      ]
[0.0,         rightVolume]
```

---

## パラメータ

### 距離
- minDistance: 1.0f（減衰開始距離）
- maxDistance: 100.0f（最大聞こえる距離）

### 音量
- baseVolume: 1.0f（基本音量）
- currentVolume: 距離減衰後の実際の音量

### その他
- speedOfSound: 343.0f（音速 m/s）
- dopplerFactor: 1.0f（ドップラー効果係数）
- dopplerScale: 1.0f（ドップラースケール）
