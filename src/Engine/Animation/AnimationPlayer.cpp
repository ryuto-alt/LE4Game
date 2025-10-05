#include "AnimationPlayer.h"
#include "AnimationUtility.h"
#include "Mymath.h"
#include <cmath>
#include <algorithm>

// コンストラクタ
AnimationPlayer::AnimationPlayer() 
    : animationTime_(0.0f), isPlaying_(false), isLoop_(true) {
}

// デストラクタ
AnimationPlayer::~AnimationPlayer() {
}

// アニメーションを設定
void AnimationPlayer::SetAnimation(const Animation& animation) {
    animation_ = animation;
    animationTime_ = 0.0f;
}

// アニメーション時刻を更新
void AnimationPlayer::Update(float deltaTime) {
    if (!isPlaying_) {
        return;
    }
    
    // 時刻を進める
    animationTime_ += deltaTime;
    
    // ループ処理またはクランプ処理
    if (isLoop_) {
        // ループする場合は時刻をリピート
        if (animation_.duration > 0.0f) {
            animationTime_ = std::fmod(animationTime_, animation_.duration);
        }
    } else {
        // ループしない場合は時刻をクランプ
        if (animationTime_ >= animation_.duration) {
            animationTime_ = animation_.duration;
            isPlaying_ = false; // アニメーション終了
        }
    }
}

// 指定したノードのローカル変換行列を取得
Matrix4x4 AnimationPlayer::GetLocalMatrix(const std::string& nodeName) {
    // 該当するNodeAnimationを検索
    auto it = animation_.nodeAnimations.find(nodeName);
    if (it == animation_.nodeAnimations.end()) {
        // 見つからない場合は単位行列を返す
        return MakeIdentity4x4();
    }
    
    const NodeAnimation& nodeAnimation = it->second;
    
    // 指定時刻の各種データを取得
    Vector3 translate = { 0.0f, 0.0f, 0.0f };
    Quaternion rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    
    // Translateの値を計算
    if (!nodeAnimation.translate.empty()) {
        translate = ::CalculateValue(nodeAnimation.translate, animationTime_);
    }
    
    // Rotateの値を計算
    if (!nodeAnimation.rotate.empty()) {
        rotate = ::CalculateValue(nodeAnimation.rotate, animationTime_);
    }
    
    // Scaleの値を計算
    if (!nodeAnimation.scale.empty()) {
        scale = ::CalculateValue(nodeAnimation.scale, animationTime_);
    }
    
    // クォータニオンから回転行列を直接生成してアフィン変換行列を作成
    return MakeAffineMatrix(scale, rotate, translate);
}

// アニメーション時刻を設定
void AnimationPlayer::SetTime(float time) {
    animationTime_ = (time > 0.0f) ? time : 0.0f;
    if (animation_.duration > 0.0f && animationTime_ > animation_.duration) {
        if (isLoop_) {
            animationTime_ = std::fmod(animationTime_, animation_.duration);
        } else {
            animationTime_ = animation_.duration;
        }
    }
}

// アニメーションの長さを取得
float AnimationPlayer::GetDuration() const {
    return animation_.duration;
}

// アニメーション再生を開始
void AnimationPlayer::Play() {
    isPlaying_ = true;
}

// アニメーション再生を停止
void AnimationPlayer::Stop() {
    isPlaying_ = false;
    // animationTime_ = 0.0f; // 時刻をリセットしないことで現在のポーズを維持
}

// アニメーション再生を一時停止
void AnimationPlayer::Pause() {
    isPlaying_ = false;
}