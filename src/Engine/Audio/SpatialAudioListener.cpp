#include "SpatialAudioListener.h"

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

SpatialAudioListener::~SpatialAudioListener() {
}

void SpatialAudioListener::SetPosition(const Vector3& position) {
    position_ = position;
}

void SpatialAudioListener::SetVelocity(const Vector3& velocity) {
    velocity_ = velocity;
}

void SpatialAudioListener::SetOrientation(const Vector3& forward, const Vector3& up) {
    forward_ = forward;
    up_ = up;
}

void SpatialAudioListener::SetMasterVolume(float volume) {
    masterVolume_ = (volume < 0.0f) ? 0.0f : (volume > 1.0f) ? 1.0f : volume;
}

void SpatialAudioListener::SetDopplerFactor(float factor) {
    dopplerFactor_ = (factor < 0.0f) ? 0.0f : (factor > 10.0f) ? 10.0f : factor;
}

void SpatialAudioListener::SetSpeedOfSound(float speed) {
    speedOfSound_ = (speed > 1.0f) ? speed : 1.0f; // 最小値を1.0fに制限
}