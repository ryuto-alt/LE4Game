#include "AnimationBlender.h"
#include "AnimationUtility.h"
#include "Mymath.h"
#include <algorithm>
#include <cmath>

// Windows.hのmin/maxマクロを無効化
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// コンストラクタ
AnimationBlender::AnimationBlender() 
    : isBlending_(false)
    , blendProgress_(0.0f)
    , blendDuration_(0.0f)
    , blendElapsedTime_(0.0f)
    , isLoop_(true) {
}

// デストラクタ
AnimationBlender::~AnimationBlender() {
}

// 初期化
void AnimationBlender::Initialize() {
    currentPlayer_ = std::make_unique<AnimationPlayer>();
    targetPlayer_ = std::make_unique<AnimationPlayer>();
}

// 更新
void AnimationBlender::Update(float deltaTime) {
    // 現在のアニメーションを更新
    if (currentPlayer_) {
        currentPlayer_->Update(deltaTime);
    }
    
    // ブレンド中の場合
    if (isBlending_ && targetPlayer_) {
        // ターゲットアニメーションも更新
        targetPlayer_->Update(deltaTime);
        
        // ブレンド進行度を更新
        blendElapsedTime_ += deltaTime;
        blendProgress_ = std::min(blendElapsedTime_ / blendDuration_, 1.0f);
        
        // ブレンド完了判定
        if (blendProgress_ >= 1.0f) {
            // ターゲットを現在のアニメーションに切り替え
            currentPlayer_ = std::move(targetPlayer_);
            targetPlayer_ = std::make_unique<AnimationPlayer>();
            targetPlayer_->SetLoop(isLoop_);
            
            isBlending_ = false;
            blendProgress_ = 0.0f;
            blendElapsedTime_ = 0.0f;
        }
    }
}

// 現在のアニメーションを設定
void AnimationBlender::SetAnimation(const Animation& animation) {
    if (!currentPlayer_) {
        currentPlayer_ = std::make_unique<AnimationPlayer>();
    }
    currentPlayer_->SetAnimation(animation);
    currentPlayer_->SetLoop(isLoop_);
    isBlending_ = false;
}

// 新しいアニメーションにブレンド遷移
void AnimationBlender::TransitionTo(const Animation& animation, float transitionDuration) {
    if (!currentPlayer_) {
        // 現在のアニメーションがない場合は直接設定
        SetAnimation(animation);
        return;
    }
    
    // ブレンド中の場合は現在のブレンド結果を固定
    if (isBlending_ && targetPlayer_) {
       
    }
    
    // ターゲットプレイヤーを初期化
    if (!targetPlayer_) {
        targetPlayer_ = std::make_unique<AnimationPlayer>();
    }
    
    targetPlayer_->SetAnimation(animation);
    targetPlayer_->SetLoop(isLoop_);
    targetPlayer_->Play();
    
    // ブレンド開始
    isBlending_ = true;
    blendDuration_ = transitionDuration;
    blendElapsedTime_ = 0.0f;
    blendProgress_ = 0.0f;
}

// 指定したノードのローカル変換行列を取得（ブレンド結果）
Matrix4x4 AnimationBlender::GetLocalMatrix(const std::string& nodeName) {
    if (!currentPlayer_) {
        return MakeIdentity4x4();
    }
    
    // ブレンド中でない場合は現在のアニメーションの結果を返す
    if (!isBlending_ || !targetPlayer_) {
        return currentPlayer_->GetLocalMatrix(nodeName);
    }
    
    // 両方のアニメーションから行列を取得
    Matrix4x4 currentMatrix = currentPlayer_->GetLocalMatrix(nodeName);
    Matrix4x4 targetMatrix = targetPlayer_->GetLocalMatrix(nodeName);
    
    // 行列を分解
    Vector3 currentScale, targetScale;
    Quaternion currentRotation, targetRotation;
    Vector3 currentTranslation, targetTranslation;
    
    // 現在のアニメーションの変換を分解
    // 各軸のベクトルの長さを計算
    Vector3 xAxis = {currentMatrix.m[0][0], currentMatrix.m[0][1], currentMatrix.m[0][2]};
    Vector3 yAxis = {currentMatrix.m[1][0], currentMatrix.m[1][1], currentMatrix.m[1][2]};
    Vector3 zAxis = {currentMatrix.m[2][0], currentMatrix.m[2][1], currentMatrix.m[2][2]};
    
    currentScale = {
        std::sqrt(xAxis.x * xAxis.x + xAxis.y * xAxis.y + xAxis.z * xAxis.z),
        std::sqrt(yAxis.x * yAxis.x + yAxis.y * yAxis.y + yAxis.z * yAxis.z),
        std::sqrt(zAxis.x * zAxis.x + zAxis.y * zAxis.y + zAxis.z * zAxis.z)
    };
    
    Matrix4x4 currentRotMat = currentMatrix;
    currentRotMat.m[0][0] /= currentScale.x; currentRotMat.m[0][1] /= currentScale.x; currentRotMat.m[0][2] /= currentScale.x;
    currentRotMat.m[1][0] /= currentScale.y; currentRotMat.m[1][1] /= currentScale.y; currentRotMat.m[1][2] /= currentScale.y;
    currentRotMat.m[2][0] /= currentScale.z; currentRotMat.m[2][1] /= currentScale.z; currentRotMat.m[2][2] /= currentScale.z;
    // 行列からクォータニオンを抽出
    float trace = currentRotMat.m[0][0] + currentRotMat.m[1][1] + currentRotMat.m[2][2];
    if (trace > 0) {
        float s = 0.5f / std::sqrt(trace + 1.0f);
        currentRotation.w = 0.25f / s;
        currentRotation.x = (currentRotMat.m[2][1] - currentRotMat.m[1][2]) * s;
        currentRotation.y = (currentRotMat.m[0][2] - currentRotMat.m[2][0]) * s;
        currentRotation.z = (currentRotMat.m[1][0] - currentRotMat.m[0][1]) * s;
    } else {
        if (currentRotMat.m[0][0] > currentRotMat.m[1][1] && currentRotMat.m[0][0] > currentRotMat.m[2][2]) {
            float s = 2.0f * std::sqrt(1.0f + currentRotMat.m[0][0] - currentRotMat.m[1][1] - currentRotMat.m[2][2]);
            currentRotation.w = (currentRotMat.m[2][1] - currentRotMat.m[1][2]) / s;
            currentRotation.x = 0.25f * s;
            currentRotation.y = (currentRotMat.m[0][1] + currentRotMat.m[1][0]) / s;
            currentRotation.z = (currentRotMat.m[0][2] + currentRotMat.m[2][0]) / s;
        } else if (currentRotMat.m[1][1] > currentRotMat.m[2][2]) {
            float s = 2.0f * std::sqrt(1.0f + currentRotMat.m[1][1] - currentRotMat.m[0][0] - currentRotMat.m[2][2]);
            currentRotation.w = (currentRotMat.m[0][2] - currentRotMat.m[2][0]) / s;
            currentRotation.x = (currentRotMat.m[0][1] + currentRotMat.m[1][0]) / s;
            currentRotation.y = 0.25f * s;
            currentRotation.z = (currentRotMat.m[1][2] + currentRotMat.m[2][1]) / s;
        } else {
            float s = 2.0f * std::sqrt(1.0f + currentRotMat.m[2][2] - currentRotMat.m[0][0] - currentRotMat.m[1][1]);
            currentRotation.w = (currentRotMat.m[1][0] - currentRotMat.m[0][1]) / s;
            currentRotation.x = (currentRotMat.m[0][2] + currentRotMat.m[2][0]) / s;
            currentRotation.y = (currentRotMat.m[1][2] + currentRotMat.m[2][1]) / s;
            currentRotation.z = 0.25f * s;
        }
    }
    
    currentTranslation = {currentMatrix.m[3][0], currentMatrix.m[3][1], currentMatrix.m[3][2]};
    
    // ターゲットアニメーションの変換を分解
    // 各軸のベクトルの長さを計算
    Vector3 targetXAxis = {targetMatrix.m[0][0], targetMatrix.m[0][1], targetMatrix.m[0][2]};
    Vector3 targetYAxis = {targetMatrix.m[1][0], targetMatrix.m[1][1], targetMatrix.m[1][2]};
    Vector3 targetZAxis = {targetMatrix.m[2][0], targetMatrix.m[2][1], targetMatrix.m[2][2]};
    
    targetScale = {
        std::sqrt(targetXAxis.x * targetXAxis.x + targetXAxis.y * targetXAxis.y + targetXAxis.z * targetXAxis.z),
        std::sqrt(targetYAxis.x * targetYAxis.x + targetYAxis.y * targetYAxis.y + targetYAxis.z * targetYAxis.z),
        std::sqrt(targetZAxis.x * targetZAxis.x + targetZAxis.y * targetZAxis.y + targetZAxis.z * targetZAxis.z)
    };
    
    Matrix4x4 targetRotMat = targetMatrix;
    targetRotMat.m[0][0] /= targetScale.x; targetRotMat.m[0][1] /= targetScale.x; targetRotMat.m[0][2] /= targetScale.x;
    targetRotMat.m[1][0] /= targetScale.y; targetRotMat.m[1][1] /= targetScale.y; targetRotMat.m[1][2] /= targetScale.y;
    targetRotMat.m[2][0] /= targetScale.z; targetRotMat.m[2][1] /= targetScale.z; targetRotMat.m[2][2] /= targetScale.z;
    // 行列からクォータニオンを抽出
    float targetTrace = targetRotMat.m[0][0] + targetRotMat.m[1][1] + targetRotMat.m[2][2];
    if (targetTrace > 0) {
        float s = 0.5f / std::sqrt(targetTrace + 1.0f);
        targetRotation.w = 0.25f / s;
        targetRotation.x = (targetRotMat.m[2][1] - targetRotMat.m[1][2]) * s;
        targetRotation.y = (targetRotMat.m[0][2] - targetRotMat.m[2][0]) * s;
        targetRotation.z = (targetRotMat.m[1][0] - targetRotMat.m[0][1]) * s;
    } else {
        if (targetRotMat.m[0][0] > targetRotMat.m[1][1] && targetRotMat.m[0][0] > targetRotMat.m[2][2]) {
            float s = 2.0f * std::sqrt(1.0f + targetRotMat.m[0][0] - targetRotMat.m[1][1] - targetRotMat.m[2][2]);
            targetRotation.w = (targetRotMat.m[2][1] - targetRotMat.m[1][2]) / s;
            targetRotation.x = 0.25f * s;
            targetRotation.y = (targetRotMat.m[0][1] + targetRotMat.m[1][0]) / s;
            targetRotation.z = (targetRotMat.m[0][2] + targetRotMat.m[2][0]) / s;
        } else if (targetRotMat.m[1][1] > targetRotMat.m[2][2]) {
            float s = 2.0f * std::sqrt(1.0f + targetRotMat.m[1][1] - targetRotMat.m[0][0] - targetRotMat.m[2][2]);
            targetRotation.w = (targetRotMat.m[0][2] - targetRotMat.m[2][0]) / s;
            targetRotation.x = (targetRotMat.m[0][1] + targetRotMat.m[1][0]) / s;
            targetRotation.y = 0.25f * s;
            targetRotation.z = (targetRotMat.m[1][2] + targetRotMat.m[2][1]) / s;
        } else {
            float s = 2.0f * std::sqrt(1.0f + targetRotMat.m[2][2] - targetRotMat.m[0][0] - targetRotMat.m[1][1]);
            targetRotation.w = (targetRotMat.m[1][0] - targetRotMat.m[0][1]) / s;
            targetRotation.x = (targetRotMat.m[0][2] + targetRotMat.m[2][0]) / s;
            targetRotation.y = (targetRotMat.m[1][2] + targetRotMat.m[2][1]) / s;
            targetRotation.z = 0.25f * s;
        }
    }
    
    targetTranslation = {targetMatrix.m[3][0], targetMatrix.m[3][1], targetMatrix.m[3][2]};
    
    // ブレンド（t * B + (1 - t) * A）
    float t = blendProgress_;
    
    // スケールの線形補間
    Vector3 blendedScale = Lerp(currentScale, targetScale, t);
    
    // 回転の球面線形補間
    Quaternion blendedRotation = Slerp(currentRotation, targetRotation, t);
    
    // 位置の線形補間
    Vector3 blendedTranslation = Lerp(currentTranslation, targetTranslation, t);
    
  
    Vector4 blendedRotationVec4 = {blendedRotation.x, blendedRotation.y, blendedRotation.z, blendedRotation.w};
    return MakeAffineMatrix(blendedScale, blendedRotationVec4, blendedTranslation);
}

// アニメーション再生制御
void AnimationBlender::Play() {
    if (currentPlayer_) {
        currentPlayer_->Play();
    }
    if (isBlending_ && targetPlayer_) {
        targetPlayer_->Play();
    }
}

void AnimationBlender::Stop() {
    if (currentPlayer_) {
        currentPlayer_->Stop();
    }
    if (targetPlayer_) {
        targetPlayer_->Stop();
    }
    isBlending_ = false;
}

void AnimationBlender::Pause() {
    if (currentPlayer_) {
        currentPlayer_->Pause();
    }
    if (isBlending_ && targetPlayer_) {
        targetPlayer_->Pause();
    }
}

void AnimationBlender::SetLoop(bool loop) {
    isLoop_ = loop;
    if (currentPlayer_) {
        currentPlayer_->SetLoop(loop);
    }
    if (targetPlayer_) {
        targetPlayer_->SetLoop(loop);
    }
}

// 現在のアニメーション時刻を取得
float AnimationBlender::GetCurrentAnimationTime() const {
    if (currentPlayer_) {
        return currentPlayer_->GetTime();
    }
    return 0.0f;
}

// アニメーション時刻を設定
void AnimationBlender::SetAnimationTime(float time) {
    if (currentPlayer_) {
        currentPlayer_->SetTime(time);
    }
}