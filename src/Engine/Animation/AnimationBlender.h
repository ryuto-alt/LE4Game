#pragma once
#include "Animation.h"
#include "AnimationPlayer.h"
#include <string>
#include <memory>

// アニメーションブレンディングクラス
class AnimationBlender {
public:
    // コンストラクタ
    AnimationBlender();
    
    // デストラクタ
    ~AnimationBlender();
    
    // 初期化
    void Initialize();
    
    // 更新
    void Update(float deltaTime);
    
    // 現在のアニメーションを設定
    void SetAnimation(const Animation& animation);
    
    // 新しいアニメーションにブレンド遷移
    void TransitionTo(const Animation& animation, float transitionDuration);
    
    // 指定したノードのローカル変換行列を取得（ブレンド結果）
    Matrix4x4 GetLocalMatrix(const std::string& nodeName);
    
    // ブレンド中かどうか
    bool IsBlending() const { return isBlending_; }
    
    // ブレンド進行度を取得（0.0〜1.0）
    float GetBlendProgress() const { return blendProgress_; }
    
    // アニメーション再生制御
    void Play();
    void Stop();
    void Pause();
    void SetLoop(bool loop);
    
    // 現在のアニメーション時刻を取得
    float GetCurrentAnimationTime() const;
    
    // アニメーション時刻を設定
    void SetAnimationTime(float time);

private:
    // 現在のアニメーションプレイヤー
    std::unique_ptr<AnimationPlayer> currentPlayer_;
    
    // 遷移先のアニメーションプレイヤー
    std::unique_ptr<AnimationPlayer> targetPlayer_;
    
    // ブレンド中フラグ
    bool isBlending_;
    
    // ブレンド進行度（0.0〜1.0）
    float blendProgress_;
    
    // ブレンド時間
    float blendDuration_;
    
    // ブレンド経過時間
    float blendElapsedTime_;
    
    // ループ設定
    bool isLoop_;
};